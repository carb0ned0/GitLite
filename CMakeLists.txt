cmake_minimum_required(VERSION 3.10)
project(GitLite)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Generate compile_commands.json for IntelliSense
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
add_executable(gitlite src/main.cpp src/git_objects.cpp src/repo.cpp)
target_link_libraries(gitlite OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
target_include_directories(gitlite PRIVATE include)