#!/usr/bin/env bash
# Wirepas Oy

 function assert_clang_version
{
    _target_version="${1}"
    _major=$(clang-format -version \
            | awk '{split($0,a," "); print a[3]}' \
            | awk '{split($0,a,"."); print a[1]}')

     if (( "${_major}" < "${_target_version}" ))
    then
        echo "Please upgrade clang-format to version +${_target_version}"
        exit 0
    fi
}
