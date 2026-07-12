#!/usr/bin/env bash
set -euo pipefail

repo="${1:-}"
if [[ -z "$repo" || ! -d "$repo/.git" ]]; then
  echo "usage: $0 /path/to/llama.cpp" >&2
  exit 2
fi

expected="e3546c7948e3af463d0b401e6421d5a4c2faf565"
actual="$(git -C "$repo" rev-parse HEAD)"
if [[ "$actual" != "$expected" ]]; then
  echo "warning: patchset was authored for $expected, current HEAD is $actual" >&2
fi

cp "$(dirname "$0")/new_files/llama-ahsma.h"   "$repo/src/llama-ahsma.h"
cp "$(dirname "$0")/new_files/llama-ahsma.cpp" "$repo/src/llama-ahsma.cpp"

for p in "$(dirname "$0")"/patches/*.patch; do
  git -C "$repo" apply --check "$p"
  git -C "$repo" apply "$p"
done

echo "AHSMA plumbing applied. Build with the normal llama.cpp CMake workflow."
echo "Runtime remains dense; LLAMA_AHSMA=1 only constructs the experimental index."
