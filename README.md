# GitLite: A Simple Git Clone in C++

Hey there! GitLite is basically a stripped-down version of Git, built with C++. Think of it as a little project to learn how Git works under the hood. It copies some of Git's main tricks for keeping track of your file changes using things like blobs, trees, and commits.

It uses the same basic ideas as the real Git, like SHA-1 hashes to name things and zlib to squish files down so they don't take up too much space. Pretty neat, huh?

## Features

Right now, GitLite knows these commands:

* `gitlite init [<path>]`: Kicks things off! It sets up a new GitLite spot (repository) in a folder. If you don't tell it where, it'll just use the folder you're in. Makes a `.git` folder just like the real Git.
* `gitlite hash-object <file>`: Takes a file, figures out its unique SHA-1 ID (hash), and saves it in the `.git/objects` folder. Then it tells you the hash it came up with.
* `gitlite cat-file <type> <object>`: Shows you what's inside a Git object (like a file's content (blob), a directory listing (tree), or commit info) if you give it the SHA-1 hash.
* `gitlite write-tree`: Looks at all the files you have right now (except for `.git` stuff, dotfiles starting with '.', and a hardcoded list of build-related files/dirs like 'gitlite', 'test.sh', 'CMakeLists.txt', 'include', 'src', etc.) and makes a 'tree' object out of them. It spits out the SHA-1 hash for that tree. Note: This uses hardcoded ignores for now; see TODOs for improvements.
* `gitlite ls-tree <tree_sha>`: Shows you what's inside a tree object – basically, a list of files and folders, their permissions, their hashes, and their names.
* `gitlite commit-tree <tree_sha> [-p <parent_commit_sha>] -m <message>`: Makes a new commit! You give it the tree hash you just made, tell it which commit came before this one (using `-p`), and write a message (using `-m`). It then gives you the SHA-1 hash for your brand-new commit.
* `gitlite log [<commit_sha>]`: Shows you the history! Starting from a specific commit (or just HEAD if you don't specify), it walks back through the parent commits and tells you about each one.
* `gitlite checkout <commit_sha>`: Time travel! This changes the files in your folder back to how they looked in that specific commit. It also makes your `HEAD` file point straight to that commit hash (this is called a 'detached HEAD' state).

## Dependencies

Before you can build and play with GitLite, make sure you've got these installed:

1. **A C++ compiler that understands C++17** (GCC, Clang, or MSVC should do the trick).
2. **CMake** (version 3.10 or newer) – This is what we use to build everything.
3. **OpenSSL Development Stuff** (On Ubuntu/Debian, it's `libssl-dev`; on Fedora/CentOS, `openssl-devel`; on Macs, `openssl` via Homebrew) – Needed for the SHA-1 hashing part.
4. **Zlib Development Stuff** (On Ubuntu/Debian, `zlib1g-dev`; on Fedora/CentOS, `zlib-devel`; on Macs, it usually comes with the command-line tools, or get it via Homebrew) – This is for compressing and uncompressing the Git objects.

## Compilation

We use CMake, so it's pretty straightforward:

1. Grab the code:

    ```bash
    git clone https://github.com/carb0ned0/GitLite
    cd GitLite
    ```

2. Make a build folder (it keeps things tidy!):

    ```bash
    mkdir build
    cd build
    ```

3. Run CMake:

    It'll check your system, find the compiler, OpenSSL, and Zlib.

    ```bash
    cmake ..
    ```

    If CMake gets stuck finding OpenSSL or Zlib, you might need to give it a hint like `-DOPENSSL_ROOT_DIR=/path/to/openssl`.

4. Compile!

    ```bash
    make
    ```

    You should now have a `gitlite` program ready to go in your `build` folder.

## Usage

After building, you run the `gitlite` program from the project root using `./build/gitlite <command>` (or from inside `build/` as `./gitlite <command>`).

### Here's a quick example

1. Start a new repo:

    ```bash
    cd .. # Go back up, or make a new folder somewhere else
    mkdir my_cool_project
    cd my_cool_project
    ../build/gitlite init # Use the path to where you built gitlite
    ```

2. Make some files:

    ```bash
    echo "First version!" > my_file.txt
    echo "Some notes here." > notes.txt
    ```

3. See how hashing works (just for fun):

    ```bash
    # Get the hash for my_file.txt and save it
    FILE_SHA=$(../build/gitlite hash-object my_file.txt)
    echo $FILE_SHA

    # See the file content using its hash
    ../build/gitlite cat-file blob $FILE_SHA
    ```

4. Snapshot the directory:

    ```bash
    TREE_SHA=$(../build/gitlite write-tree)
    echo $TREE_SHA
    ```

5. See what's in the snapshot:

    ```bash
    ../build/gitlite ls-tree $TREE_SHA
    ```

6. Make your first commit:

    ```bash
    COMMIT_SHA=$(../build/gitlite commit-tree $TREE_SHA -m "My first commit, yay!")
    echo $COMMIT_SHA
    ```

7. Check the log:

    ```bash
    ../build/gitlite log $COMMIT_SHA
    # Or just: ../build/gitlite log HEAD
    ```

8. Change something and commit again:

    ```bash
    echo "Second version!" > my_file.txt
    NEW_TREE_SHA=$(../build/gitlite write-tree)
    NEW_COMMIT_SHA=$(../build/gitlite commit-tree $NEW_TREE_SHA -p $COMMIT_SHA -m "Updated my file")
    echo $NEW_COMMIT_SHA
    ../build/gitlite log $NEW_COMMIT_SHA # See both commits now!
    ```

9. Go back to the first version:

    ```bash
    ../build/gitlite checkout $COMMIT_SHA # Use the first commit's hash
    cat my_file.txt # Should say "First version!" again
    ```

## Testing it Out

There's a handy script `test.sh` that runs through the basic commands to make sure they're working okay. It now creates a temporary directory (`temp_test_dir`) to isolate the tests (avoiding interference from project files), runs the tests there, and cleans it up afterward.

1. Make sure you've built the project first! (You need `./build/gitlite`).
2. From the main project folder (where `test.sh` is), run:

```bash
./test.sh
```

![Test.sh run example](/test%20output.png)

It'll run a bunch of commands and print "OK" if things look good, or an error if something seems broken. Fingers crossed!
