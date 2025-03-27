#!/usr/bin/env bash

cd $(realpath -m "$0/../../..")

set -ex

check_gen_po()
{
    bjam update-po
    if grep -E '^([-+]#: |@@|[-+]"|diff |index |[-]{3} |[+]{3})' -qv < <(
        git diff --unified=0 ./tools/i18n/po/*/
    ); then
        echo 'Error: .po files are outdated (run bjam update-po)'
        return 1
    fi
}

check_gen_logid()
{
    ./tools/log_siem/extractor.py -Cp > "$TMPDIR_TEST"/siem_filters_rdp_proxy.py
    diff ./tools/log_siem/siem_filters_rdp_proxy.py "$TMPDIR_TEST"/siem_filters_rdp_proxy.py
}

check_gen_config()
{
    (
        cd projects/redemption_configs
        bjam
        declare r=$(git status -s . 2>&1)
        if [[ -n $r ]]; then
            echo "$r"
            return 1
        fi
    )
}

errors=()
checkers=(
    check_gen_po
    check_gen_logid
    check_gen_config
)
for f in "${checkers[@]}"; do
     "$f" || errors+=("$f")
done

if (( ${#errors} )); then
    printf 'error: %s\n' "${errors[@]}"
    exit ${#errors}
fi
