#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>  // For uint8_t, etc.

// Base Git Object
class GitObject {
public:
    std::string fmt;  // "blob", "tree", "commit"
    virtual std::string serialize() const = 0;
    virtual ~GitObject() = default;
};

// Blob (file content)
class GitBlob : public GitObject {
public:
    std::string blobdata;
    GitBlob() { fmt = "blob"; }
    std::string serialize() const override { return blobdata; }
};

// Tree Leaf (file or sub-tree in a tree)
struct GitTreeLeaf {
    uint32_t mode;       // e.g., 100644 for file
    std::string path;    // Filename
    std::string sha;     // SHA-1 hex
};

// Tree (directory)
class GitTree : public GitObject {
public:
    std::vector<GitTreeLeaf> items;
    GitTree() { fmt = "tree"; }
    std::string serialize() const override;
    static GitTree parse(const std::string& data);  // Added
};

// Commit
class GitCommit : public GitObject {
public:
    std::vector<std::pair<std::string, std::string>> kvlm;  // Key-value list message (preserves order)
    GitCommit() { fmt = "commit"; }
    std::string serialize() const override;
    static GitCommit parse(const std::string& data);  // Added
};