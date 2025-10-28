#!/bin/bash

# Clean up previous .git and test files
rm -rf .git temp_dir
rm -f test.txt hello.txt

# Test init
./gitlite init
if [ ! -d ".git" ]; then
    echo "Error: init failed"
    exit 1
fi
echo "init: OK"

# Create test files
echo "test" > test.txt
echo "hello world" > hello.txt

# Test hash-object and cat-file
blob_sha=$(./gitlite hash-object test.txt)
echo "Blob SHA (test.txt): $blob_sha"
cat_output=$(./gitlite cat-file blob $blob_sha)
if [ "$cat_output" != "test" ]; then
    echo "Error: cat-file failed for blob"
    exit 1
fi
echo "hash-object and cat-file: OK"

# Test type mismatch in cat-file
if ./gitlite cat-file tree $blob_sha 2>&1 | grep -q "Object type mismatch"; then
    echo "cat-file type validation: OK"
else
    echo "Error: cat-file type validation failed"
    exit 1
fi

# Test write-tree
tree_sha=$(./gitlite write-tree)
echo "Tree SHA: $tree_sha"
ls_tree_output=$(./gitlite ls-tree $tree_sha | sort)
expected_ls_tree=$(echo -e "100644 hello.txt\t3b18e512dba79e4c8300dd08aeb37f8e728b8dad\n100644 test.txt\t9daeafb9864cf43055ae93beb0afd6c7d144bfa4" | sort)
if [ "$ls_tree_output" != "$expected_ls_tree" ]; then
    echo "Error: ls-tree output mismatch"
    echo "Expected: $expected_ls_tree"
    echo "Got: $ls_tree_output"
    exit 1
fi
echo "write-tree and ls-tree: OK"

# Test commit-tree (first commit)
commit_sha1=$(./gitlite commit-tree $tree_sha -m "Initial commit")
echo "Commit SHA1: $commit_sha1"
log_output1=$(./gitlite log $commit_sha1)
if ! echo "$log_output1" | grep -q "commit $commit_sha1" || ! echo "$log_output1" | grep -q "Initial commit"; then
    echo "Error: log failed for first commit"
    exit 1
fi
echo "commit-tree and log (first): OK"

# Test second commit with parent
commit_sha2=$(./gitlite commit-tree $tree_sha -p $commit_sha1 -m "Second commit")
echo "Commit SHA2: $commit_sha2"
log_output2=$(./gitlite log $commit_sha2)
if ! echo "$log_output2" | grep -q "Second commit" || ! echo "$log_output2" | grep -q "Initial commit"; then
    echo "Error: log failed for commit chain"
    exit 1
fi
echo "commit-tree and log (chain): OK"

# Test checkout (remove files, checkout back)
rm test.txt hello.txt
./gitlite checkout $commit_sha2
if [ ! -f "test.txt" ] || [ ! -f "hello.txt" ]; then
    echo "Error: checkout failed to restore files"
    exit 1
fi
head_content=$(cat .git/HEAD)
if [ "$head_content" != "$commit_sha2" ]; then
    echo "Error: checkout failed to update HEAD"
    exit 1
fi
echo "checkout: OK"

# Test log from HEAD
log_from_head=$(./gitlite log)
if [ "$log_from_head" != "$log_output2" ]; then
    echo "Error: log from HEAD failed"
    exit 1
fi
echo "log from HEAD: OK"

echo "All tests passed!"