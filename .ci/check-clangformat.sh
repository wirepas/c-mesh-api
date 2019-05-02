#!/usr/bin/env bash
# Wirepas Oy
set -e

TARGET_CLANG_FORMAT=${TARGET_CLANG_FORMAT:-"7"}

 # shellcheck disable=SC1091
source ./.ci/modules/asserts.sh
assert_clang_version "${TARGET_CLANG_FORMAT}"

_file_list=$(find . -type f \( -name "*.c" -o -name "*.h*" \))

ret=0
for _file in ${_file_list}
do
    #shellcheck disable=SC2094
  _lines=$(clang-format -style=file < "${_file}" | diff "${_file}" - | wc -l)
  echo "$_file : $_lines"

  if (( "${_lines}" > 0 ))
  then
    ((ret=ret+_lines))
  fi
done

echo "Found $ret issues"
exit "$ret"
