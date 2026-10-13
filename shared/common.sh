#!/bin/bash
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHARED="$ROOT/shared"
CXXFLAGS_NATIVE="-O3 -mcpu=native -std=c++17 -w -I/opt/homebrew/include -I/opt/homebrew/opt/simde/include"
CXXFLAGS_X86="-arch x86_64 -O3 -march=haswell -std=c++17 -w -I/opt/homebrew/include"

solution_dir() {
  local s="${1%/}"
  if [ -z "$s" ] || [ ! -d "$ROOT/$s/src" ]; then
    echo "error: no solution folder '$s' (expected $ROOT/$s/src)" >&2
    exit 1
  fi
  echo "$ROOT/$s"
}
