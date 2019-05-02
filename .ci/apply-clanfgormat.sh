#!/usr/bin/env bash
# Wirepas Oy

set -e

TARGET_CLANG_FORMAT=${TARGET_CLANG_FORMAT:-"7"}

 # shellcheck source=./.ci/modules/asserts.sh
source ./.ci/modules/asserts.sh
assert_clang_version "${TARGET_CLANG_FORMAT}"

_file_list=$(find . -type f \( -name "*.c" -o -name "*.h*" \))

for _file in ${_file_list}
do
  echo "formatting $_file"
  clang-format -style=file -i "$_file"
done
