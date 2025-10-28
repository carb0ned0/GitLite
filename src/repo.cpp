#include "repo.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <array>
#include <stdexcept>
#include <vector>
#include <openssl/sha.h>
#include <zlib.h>
#include <iostream>
#include <filesystem>
#include <algorithm>  // Added
#include <ctime>  // Added
#include <set>  // Added for ignore set

namespace fs = std::filesystem;

// Constructor
GitRepository::GitRepository(const fs::path& path, bool create) : worktree(path), gitdir(path / ".git") {
    if (!create && !fs::exists(gitdir)) {
        throw std::runtime_error("Not a Git repository: " + path.string());
    }
    // Load config here if needed
}

// Create repo
GitRepository GitRepository::create(const fs::path& path) {
    GitRepository repo(path, true); // Skip existence check
    if (fs::exists(repo.gitdir)) {
        if (!fs::is_directory(repo.gitdir)) {
            throw std::runtime_error(".git exists but is not a directory");
        }
    } else {
        fs::create_directories(repo.gitdir);
    }

    // Use create_directories to ensure all parent directories are created
    fs::create_directories(repo.gitdir / "branches");
    fs::create_directories(repo.gitdir / "objects");
    fs::create_directories(repo.gitdir / "refs" / "tags");
    fs::create_directories(repo.gitdir / "refs" / "heads");

    std::ofstream config(repo.gitdir / "config");
    config << "[core]\n\trepositoryformatversion = 0\n\tfilemode = true\n\tbare = false\n\tlogallrefupdates = true\n";
    config.close();

    std::ofstream head(repo.gitdir / "HEAD");
    head << "ref: refs/heads/master\n";
    head.close();

    return repo;
}

// Find repo (climb up directories)
GitRepository GitRepository::find(const fs::path& path) {
    fs::path abs_path = fs::absolute(path);
    while (abs_path != abs_path.root_path()) {
        if (fs::exists(abs_path / ".git")) {
            return GitRepository(abs_path); // create = false, checks for .git
        }
        abs_path = abs_path.parent_path();
    }
    throw std::runtime_error("No Git directory found");
}

// Helper: Build path in .git
fs::path repo_path(const GitRepository& repo, const std::vector<std::string>& parts) {
    fs::path p = repo.gitdir;
    for (const auto& part : parts) {
        p /= part;
    }
    return p;
}

// repo_file: Create file path, optionally mkdir
fs::path repo_file(const GitRepository& repo, const std::vector<std::string>& parts, bool mkdir) {
    auto dir_parts = parts;
    if (!dir_parts.empty()) {
        dir_parts.pop_back();  // Last is file
    }
    repo_dir(repo, dir_parts, mkdir);
    return repo_path(repo, parts);
}

// repo_dir: Create dir if needed
fs::path repo_dir(const GitRepository& repo, const std::vector<std::string>& parts, bool mkdir) {
    auto path = repo_path(repo, parts);
    if (fs::exists(path)) {
        if (fs::is_directory(path)) return path;
        throw std::runtime_error(path.string() + " is not a directory");
    }
    if (mkdir) {
        fs::create_directories(path);
        return path;
    }
    return "";
}

// Helper: Read full decompressed object and parse fmt and data
std::pair<std::string, std::string> read_object_fmt_and_data(const GitRepository& repo, const std::string& sha) {
    std::string sha_prefix = sha.substr(0, 2);
    std::string sha_rest = sha.substr(2);
    fs::path path = repo.gitdir / "objects" / sha_prefix / sha_rest;

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open object");

    std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Decompress with zlib
    z_stream zs{};
    zs.next_in = compressed.data();
    zs.avail_in = compressed.size();

    std::vector<uint8_t> raw(4096);
    std::string decompressed;
    int ret = inflateInit(&zs);
    if (ret != Z_OK) throw std::runtime_error("zlib inflateInit error");

    ret = Z_OK;
    while (ret == Z_OK) {
        zs.next_out = raw.data();
        zs.avail_out = raw.size();
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret < 0) throw std::runtime_error("zlib inflate error");
        decompressed.append(reinterpret_cast<char*>(raw.data()), raw.size() - zs.avail_out);
    }
    inflateEnd(&zs);
    if (ret != Z_STREAM_END) throw std::runtime_error("Decompression did not reach the end of stream");

    // Parse header
    size_t space_pos = decompressed.find(' ');
    if (space_pos == std::string::npos) throw std::runtime_error("Malformed object header");
    size_t null_pos = decompressed.find('\0', space_pos + 1);
    if (null_pos == std::string::npos) throw std::runtime_error("Malformed object header");
    std::string fmt = decompressed.substr(0, space_pos);
    size_t size = std::stoi(decompressed.substr(space_pos + 1, null_pos - space_pos - 1));
    std::string data = decompressed.substr(null_pos + 1);

    if (data.length() != size) throw std::runtime_error("Size mismatch");

    return {fmt, data};
}

// Object read: Return data without header
std::string object_read(const GitRepository& repo, const std::string& sha) {
    auto [fmt, data] = read_object_fmt_and_data(repo, sha);
    return data;
}

// Object write: Serialize, compress, hash, store
std::string object_write(GitObject* obj, GitRepository* repo) {
    std::string data = obj->serialize();
    std::string result = obj->fmt + " " + std::to_string(data.length()) + '\0' + data;
    // SHA1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(result.c_str()), result.length(), hash);
    std::ostringstream sha_ss;
    sha_ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        sha_ss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    std::string sha = sha_ss.str();

    if (repo) {
        // Compress
        z_stream zs{};
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(result.c_str()));
        zs.avail_in = result.length();

        std::vector<uint8_t> compressed(4096);
        std::string out_str;
        int ret = deflateInit(&zs, Z_BEST_COMPRESSION);
        if (ret != Z_OK) throw std::runtime_error("zlib deflateInit error");

        ret = Z_OK;
        while (ret == Z_OK) {
            zs.next_out = compressed.data();
            zs.avail_out = compressed.size();
            ret = deflate(&zs, Z_FINISH);
            if (ret < 0) throw std::runtime_error("zlib deflate error");
            out_str.append(reinterpret_cast<char*>(compressed.data()), compressed.size() - zs.avail_out);
        }
        deflateEnd(&zs);
        if (ret != Z_STREAM_END) throw std::runtime_error("Compression did not reach the end of stream");

        // Write to objects/<prefix>/<rest>
        std::string prefix = sha.substr(0, 2);
        std::string rest = sha.substr(2);
        fs::path path = repo->gitdir / "objects" / prefix / rest;
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(out_str.c_str(), out_str.length());
        file.close();
    }

    return sha;
}

// Object hash: From stream
std::string object_hash(std::istream& fd, const std::string& fmt, GitRepository* repo) {
    std::string data((std::istreambuf_iterator<char>(fd)), std::istreambuf_iterator<char>());
    GitBlob obj;  // Assuming blob for now
    obj.fmt = fmt;
    obj.blobdata = data;
    return object_write(&obj, repo);
}

// Improved object_find: resolve refs and HEAD
std::string object_find(const GitRepository& repo, const std::string& name, const std::string& fmt, bool follow) {
    if (name == "HEAD") {
        std::ifstream head_file(repo.gitdir / "HEAD");
        if (!head_file) throw std::runtime_error("No HEAD");
        std::string line;
        std::getline(head_file, line);
        if (line.find("ref: ") == 0) {
            return object_find(repo, line.substr(5), fmt, follow);
        } else {
            return line;  // Detached HEAD (direct SHA)
        }
    }

    // Check if it's a ref (e.g., refs/heads/master)
    fs::path ref_path = repo.gitdir / name;
    if (fs::exists(ref_path)) {
        std::ifstream ref_file(ref_path);
        std::string sha;
        std::getline(ref_file, sha);
        if (follow) {
            return object_find(repo, sha, fmt, follow);  // Recurse if needed
        } else {
            return sha;
        }
    }

    // Assume full SHA for now (short SHA support can be added later)
    // TODO: Validate if object exists and matches fmt
    return name;
}

// New: Recursive write_tree
std::string write_tree(const GitRepository& repo, const fs::path& dir) {
    std::set<std::string> ignore = {"gitlite", "test.sh", "CMakeLists.txt", "Makefile", "cmake_install.cmake", "CMakeCache.txt", "compile_commands.json", "include", "src", "CMakeFiles", "repomix-output.xml"};
    std::vector<GitTreeLeaf> entries;
    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string filename = entry.path().filename().string();
        if (filename[0] == '.') continue;
        if (ignore.count(filename)) continue;  // Ignore build files and subdirs
        if (entry.is_directory()) {
            std::string sub_tree_sha = write_tree(repo, entry.path());
            entries.push_back({040000, filename, sub_tree_sha});
        } else if (entry.is_regular_file()) {
            std::ifstream fd(entry.path(), std::ios::binary);
            if (!fd) throw std::runtime_error("Failed to open file: " + entry.path().string());
            std::string blob_sha = object_hash(fd, "blob", const_cast<GitRepository*>(&repo));
            entries.push_back({0100644, filename, blob_sha});
        }
    }

    // Sort entries by path
    std::sort(entries.begin(), entries.end(), [](const GitTreeLeaf& a, const GitTreeLeaf& b) {
        return a.path < b.path;
    });

    GitTree tree_obj;
    tree_obj.items = entries;
    return object_write(&tree_obj, const_cast<GitRepository*>(&repo));
}

// New: Recursive read_tree for checkout
void read_tree(const GitRepository& repo, const std::string& tree_sha, const fs::path& base_path) {
    auto [fmt, tree_data] = read_object_fmt_and_data(repo, tree_sha);
    if (fmt != "tree") throw std::runtime_error("Not a tree object");
    GitTree tree = GitTree::parse(tree_data);

    for (const auto& leaf : tree.items) {
        fs::path path = base_path / leaf.path;
        if (leaf.mode == 040000) {  // Directory (tree)
            fs::create_directories(path);
            read_tree(repo, leaf.sha, path);
        } else {  // File (blob)
            auto [blob_fmt, blob_data] = read_object_fmt_and_data(repo, leaf.sha);
            if (blob_fmt != "blob") throw std::runtime_error("Not a blob object");
            std::ofstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Failed to write file: " + path.string());
            file << blob_data;
            file.close();
        }
    }
}

// Command: init
void cmd_init(const std::vector<std::string>& args) {
    fs::path path = args.empty() ? "." : args[0];
    GitRepository::create(path);
    std::cout << "Initialized git directory" << std::endl;
}

// Command: hash-object
void cmd_hash_object(const std::vector<std::string>& args) {
    if (args.empty()) throw std::runtime_error("No file provided");
    std::ifstream file(args[0], std::ios::binary);
    if (!file) throw std::runtime_error("File not found");
    GitRepository repo = GitRepository::find();
    std::string sha = object_hash(file, "blob", &repo);
    std::cout << sha << std::endl;
}

// Command: cat-file
void cmd_cat_file(const std::vector<std::string>& args) {
    if (args.size() < 2) throw std::runtime_error("Usage: cat-file <type> <object>");
    std::string type = args[0];
    std::string obj_name = args[1];
    GitRepository repo = GitRepository::find();
    std::string sha = object_find(repo, obj_name, type);
    auto [actual_type, data] = read_object_fmt_and_data(repo, sha);
    if (actual_type != type) throw std::runtime_error("Object type mismatch: expected " + type + ", got " + actual_type);
    std::cout << data;
}

// New Command: write-tree
void cmd_write_tree(const std::vector<std::string>& args) {
    GitRepository repo = GitRepository::find();
    std::string tree_sha = write_tree(repo, repo.worktree);
    std::cout << tree_sha << std::endl;
}

// New Command: commit-tree
void cmd_commit_tree(const std::vector<std::string>& args) {
    if (args.size() < 3) throw std::runtime_error("Usage: commit-tree <tree_sha> [-p <parent>] -m <message>");

    std::string tree_sha = args[0];
    std::string parent_sha;
    std::string message;
    size_t i = 1;
    if (args[i] == "-p") {
        if (args.size() < 5) throw std::runtime_error("Missing parent or message");
        parent_sha = args[i + 1];
        i += 2;
    }
    if (i >= args.size() || args[i] != "-m") throw std::runtime_error("Missing -m");
    if (i + 1 >= args.size()) throw std::runtime_error("Missing message");
    message = args[i + 1];
    if (i + 2 != args.size()) throw std::runtime_error("Extra arguments provided");

    GitCommit commit;
    commit.kvlm.push_back({"tree", tree_sha});
    if (!parent_sha.empty()) {
        commit.kvlm.push_back({"parent", parent_sha});
    }
    std::time_t now = std::time(nullptr);
    std::string timestamp = std::to_string(now) + " +0000";  // UTC
    std::string author_str = "User <user@example.com> " + timestamp;
    commit.kvlm.push_back({"author", author_str});
    commit.kvlm.push_back({"committer", author_str});
    commit.kvlm.push_back({"", message + "\n"});  // Ensure newline

    GitRepository repo = GitRepository::find();
    std::string commit_sha = object_write(&commit, &repo);
    std::cout << commit_sha << std::endl;
}

// New Command: ls-tree
void cmd_ls_tree(const std::vector<std::string>& args) {
    if (args.empty()) throw std::runtime_error("Usage: ls-tree <tree_sha>");
    std::string name = args[0];
    GitRepository repo = GitRepository::find();
    std::string sha = object_find(repo, name, "tree", true);
    auto [fmt, data] = read_object_fmt_and_data(repo, sha);
    if (fmt != "tree") throw std::runtime_error("Not a tree object");
    try {
        GitTree tree = GitTree::parse(data);
        for (const auto& leaf : tree.items) {
            std::ostringstream mode_ss;
            mode_ss << std::oct << leaf.mode;
            std::cout << mode_ss.str() << " " << leaf.path << "\t" << leaf.sha << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing tree: " << e.what() << std::endl;
        std::cerr << "Tree data length: " << data.length() << std::endl;
        std::cerr << "First 20 bytes (hex): ";
        for (size_t i = 0; i < std::min(data.length(), size_t(20)); ++i) {
            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(static_cast<unsigned char>(data[i])) << " ";
        }
        std::cerr << std::endl;
        throw;
    }
}

// New Command: log
void cmd_log(const std::vector<std::string>& args) {
    GitRepository repo = GitRepository::find();
    std::string sha = args.empty() ? "HEAD" : args[0];
    sha = object_find(repo, sha, "commit", true);

    while (!sha.empty()) {
        auto [fmt, data] = read_object_fmt_and_data(repo, sha);
        if (fmt != "commit") throw std::runtime_error("Not a commit object");
        GitCommit commit = GitCommit::parse(data);

        std::cout << "commit " << sha << std::endl;

        std::string author;
        for (const auto& kv : commit.kvlm) {
            if (kv.first == "author") {
                author = kv.second;
                break;
            }
        }
        if (!author.empty()) {
            std::cout << "Author: " << author << std::endl;
        }

        std::string message;
        for (const auto& kv : commit.kvlm) {
            if (kv.first.empty()) {
                message = kv.second;
                break;
            }
        }
        std::cout << std::endl << message << std::endl;

        std::string parent;
        for (const auto& kv : commit.kvlm) {
            if (kv.first == "parent") {
                parent = kv.second;
                break;
            }
        }
        sha = parent;
    }
}
// New Command: checkout
void cmd_checkout(const std::vector<std::string>& args) {
    if (args.empty()) throw std::runtime_error("Usage: checkout <commit_sha>");
    std::string name = args[0];
    GitRepository repo = GitRepository::find();
    std::string commit_sha = object_find(repo, name, "commit", true);
    auto [fmt, commit_data] = read_object_fmt_and_data(repo, commit_sha);
    if (fmt != "commit") throw std::runtime_error("Not a commit object");
    GitCommit commit = GitCommit::parse(commit_data);

    std::string tree_sha;
    for (const auto& kv : commit.kvlm) {
        if (kv.first == "tree") {
            tree_sha = kv.second;
            break;
        }
    }
    if (tree_sha.empty()) throw std::runtime_error("No tree in commit");

    // Checkout tree to worktree
    read_tree(repo, tree_sha, repo.worktree);

    // Update HEAD to this commit (detached)
    std::ofstream head_file(repo.gitdir / "HEAD");
    head_file << commit_sha;
    head_file.close();
}