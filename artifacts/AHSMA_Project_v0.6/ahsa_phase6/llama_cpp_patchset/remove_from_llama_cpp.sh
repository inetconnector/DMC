#!/usr/bin/env bash
set -euo pipefail
repo="${1:-}"
if [[ -z "$repo" || ! -d "$repo/.git" ]]; then
  echo "usage: $0 /path/to/llama.cpp" >&2
  exit 2
fi
git -C "$repo" reset --hard HEAD
git -C "$repo" clean -f src/llama-ahsma.h src/llama-ahsma.cpp
