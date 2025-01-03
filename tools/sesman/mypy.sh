#!/usr/bin/env bash
if [[ -z $1 ]]; then
  echo "usage: $0 <sesmanlink path>"
  exit 1
fi

curr=$(dirname $0)
sesmanlink=$1
shift

# MYPY_FORCE_COLOR=1
MYPYPATH="$sesmanlink/wallix_imports:$curr:$sesmanlink" \
  "${MYPY:-mypy}" --python-version 3.11 "$curr"/WABRDPAuthentifier "$@"
# --no-strict-optional
