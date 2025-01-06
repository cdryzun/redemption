#!/usr/bin/env sh

cd "$(dirname "$0")"/../..

errcode=0
ruff=${RUFF:-ruff}

# sources only (exclude tests)
"$ruff" check --preview "$@" \
    tools/*.py \
    tools/conf_migration_tool/*.py \
    tools/gen_keylayouts/ \
    tools/i18n/*.py \
    tools/icap_validator/ \
    tools/log_siem/ \
    tools/passthrough/ \
    tools/sesman/*.py \
    tools/sesman/benchmark/ \
    tools/sesman/sesmanworker/ \
    tools/syslog/ \
    || errcode=$?

# tests only (exclude sources)
# S104: hardcoded-bind-all-interfaces
"$ruff" check --preview --ignore=S104 "$@" \
    tools/conf_migration_tool/tests \
    tools/sesman/tests \
    || errcode=$?

exit $errcode
