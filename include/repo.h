#pragma once
#include <string>
#include <filesystem>
#include "git_objects.h"

namespace fs = std::filesystem;

class GitRepository {
public:
    fs::path worktree;
    fs::path gitdir;
    GitRepository(const fs::path& path, bool create = false);
    static GitRepository create(const fs::path& path);
    static GitRepository find(const fs::path& path = ".");
    // Other members...
};

// Utility functions
fs::path repo_path(const GitRepository& repo, const std::vector<std::string>& parts);
fs::path repo_file(const GitRepository& repo, const std::vector<std::string>& parts, bool mkdir = false);
fs::path repo_dir(const GitRepository& repo, const std::vector<std::string>& parts, bool mkdir = false);

// Object functions
std::string object_read(const GitRepository& repo, const std::string& sha);
std::string object_write(GitObject* obj, GitRepository* repo = nullptr);
std::string object_find(const GitRepository& repo, const std::string& name, const std::string& fmt = "", bool follow = true);
std::string object_hash(std::istream& fd, const std::string& fmt, GitRepository* repo = nullptr);

// Tree and checkout helpers (new)
std::string write_tree(const GitRepository& repo, const fs::path& dir);
void read_tree(const GitRepository& repo, const std::string& tree_sha, const fs::path& base_path);

// Commands (bridges)
void cmd_init(const std::vector<std::string>& args);
void cmd_hash_object(const std::vector<std::string>& args);
void cmd_cat_file(const std::vector<std::string>& args);
void cmd_write_tree(const std::vector<std::string>& args);
void cmd_commit_tree(const std::vector<std::string>& args);
void cmd_ls_tree(const std::vector<std::string>& args);
void cmd_log(const std::vector<std::string>& args);
void cmd_checkout(const std::vector<std::string>& args);