#!/usr/bin/env bash

cd $(realpath -m "$0/../..")

set -ex

# check .po files
bjam update-po
if grep -E '^([-+]#: |@@|[-+]"|diff |index |[-]{3} |[+]{3})' -qv < <(
    git diff --unified=0 ./tools/i18n/po/*/
); then
    echo 'Error: .po files are outdated (run bjam update-po)'
    exit 1
fi
