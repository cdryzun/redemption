#!/usr/bin/env bash

cd "$(dirname "$0")"/../..

set -ex

COVERAGE=${COVERAGE:-coverage}

paths=(
    tools/sesman
    tools/conf_migration_tool
)
for path in "${paths[@]}"; do
    pushd "$path"
    "$COVERAGE" run -m pytest tests
    popd
done

"$COVERAGE" combine "${paths[@]/%//.coverage}"
"$COVERAGE" report

if [[ -n "${1:-}" ]]; then
    "$COVERAGE" xml -o "${1:-}"
fi
