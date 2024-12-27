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

# check LogId constant generation
./tools/log_siem/extractor.py -Cp > "$TMPDIR_TEST"/siem_filters_rdp_proxy.py
diff ./tools/log_siem/siem_filters_rdp_proxy.py "$TMPDIR_TEST"/siem_filters_rdp_proxy.py
