#!/usr/bin/env sh

cd "$(realpath -m "$0"/../../..)"

find src \( -name '*.hpp' -or -name '*.cpp' \) \
    -and -not -path 'src/keyboard/keylayouts.cpp' \
    -exec ./tools/c++-analyzer/lua-checker/lua-checker.lua --checks \* '{}' '+'
