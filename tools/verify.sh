#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if (( $# > 1 )) || [[ "${1:-}" != "" && "${1:-}" != gcc && "${1:-}" != clang ]]; then
  printf '用法：%s [gcc|clang]\n' "$0" >&2
  exit 2
fi
compilers=(gcc clang)
if (( $# == 1 )); then
  compilers=("$1")
fi
build_dir="$(mktemp -d)"
trap 'rm -rf -- "$build_dir"' EXIT
for compiler in "${compilers[@]}"; do
  command -v "$compiler" >/dev/null
  log_file="$build_dir/$compiler.log"
  if ! {
    cmake -S "$repo_root/tests/zero-window" -B "$build_dir/$compiler" \
      -DCMAKE_C_COMPILER="$compiler" \
      -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' &&
    cmake --build "$build_dir/$compiler" &&
    (cd -- "$build_dir/$compiler" && ctest --output-on-failure)
  } >"$log_file" 2>&1; then
    printf 'ESP lwIP 验证\n  结果      失败\n  编译器    %s\n' "$compiler" >&2
    cat "$log_file" >&2
    exit 1
  fi
  printf 'ESP lwIP 验证\n  编译器    %s\n  版本      %s\n  零窗口／回绕  2/2 通过\n' \
    "$compiler" "$("$compiler" --version | head -n 1)"
done
printf '  sanitizer   ASan/UBSan\n  实板与发布  未执行\n'
