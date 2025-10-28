// src/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <openssl/sha.h>
#include <zlib.h>
#include "git_objects.h"
#include "repo.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: gitlite <command> [<args>]" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    try {
        if (command == "init") {
            cmd_init(args);
        } else if (command == "hash-object") {
            cmd_hash_object(args);
        } else if (command == "cat-file") {
            cmd_cat_file(args);
        } else if (command == "write-tree") {
            cmd_write_tree(args);
        } else if (command == "commit-tree") {
            cmd_commit_tree(args);
        } else if (command == "ls-tree") {
            cmd_ls_tree(args);
        } else if (command == "log") {
            cmd_log(args);
        } else if (command == "checkout") {
            cmd_checkout(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }

    return 0;
}