#include "git_objects.h"
#include <sstream>
#include <iomanip>  // For std::hex in parse

// Tree serialize: mode<SP>path<NULL>sha (binary SHA)
std::string GitTree::serialize() const {
    std::string ret;
    for (const auto& leaf : items) {
        std::ostringstream mode_ss;
        mode_ss << std::oct << leaf.mode;  // Octal mode
        ret += mode_ss.str() + " " + leaf.path;
        ret += '\0';  // Explicitly append null char
        // Convert hex SHA to binary
        for (size_t i = 0; i < leaf.sha.length(); i += 2) {
            uint8_t byte = static_cast<uint8_t>(std::stoi(leaf.sha.substr(i, 2), nullptr, 16));
            ret += static_cast<char>(byte);
        }
    }
    return ret;
}

GitTree GitTree::parse(const std::string& data) {
    GitTree tree;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t space_pos = data.find(' ', pos);
        if (space_pos == std::string::npos) {
            throw std::runtime_error("No space found after mode at position " + std::to_string(pos));
        }
        std::string mode_str = data.substr(pos, space_pos - pos);
        if (mode_str.empty()) {
            throw std::runtime_error("Empty mode string at position " + std::to_string(pos));
        }
        if (!std::all_of(mode_str.begin(), mode_str.end(), ::isdigit)) {
            throw std::runtime_error("Mode string contains non-digit characters: '" + mode_str + "'");
        }
        uint32_t mode = std::stoul(mode_str, nullptr, 8);  // Octal
        size_t null_pos = data.find('\0', space_pos + 1);
        if (null_pos == std::string::npos) {
            throw std::runtime_error("No null terminator found after path");
        }
        std::string path = data.substr(space_pos + 1, null_pos - space_pos - 1);
        pos = null_pos + 1;
        if (pos + 20 > data.size()) {
            throw std::runtime_error("Not enough data for SHA at position " + std::to_string(pos));
        }
        std::ostringstream sha_ss;
        sha_ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 20; ++i) {
            uint8_t byte = static_cast<uint8_t>(data[pos++]);
            sha_ss << std::setw(2) << static_cast<unsigned>(byte);
        }
        std::string sha = sha_ss.str();
        tree.items.push_back({mode, path, sha});
    }
    return tree;
}

// Commit serialize: key-value pairs + message (in insertion order)
std::string GitCommit::serialize() const {
    std::string ret;
    for (const auto& kv : kvlm) {
        if (kv.first.empty()) {  // Message
            ret += "\n" + kv.second;
        } else {
            ret += kv.first + " " + kv.second + "\n";
        }
    }
    return ret;
}

GitCommit GitCommit::parse(const std::string& data) {
    GitCommit commit;
    size_t pos = 0;
    std::string key;
    while (pos < data.size()) {
        size_t nl_pos = data.find('\n', pos);
        if (nl_pos == pos) {  // Blank line separates headers from message
            pos = nl_pos + 1;
            break;
        }
        std::string line = data.substr(pos, nl_pos - pos);
        pos = nl_pos + 1;

        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos) {
            // Continuation line (multi-line value), but for simplicity, assume single-line headers
            continue;
        } else {
            key = line.substr(0, space_pos);
            std::string value = line.substr(space_pos + 1);
            commit.kvlm.push_back({key, value});
        }
    }
    commit.kvlm.push_back({"", data.substr(pos)});  // Message
    return commit;
}