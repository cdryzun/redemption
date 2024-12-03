#!/usr/bin/env bash

set -eu

usage_and_exit() {
    echo >&2 "Usage:
    $0 init <locale>
    $0 extract
    $0 merge ([<locale>] | 'all')
    $0 compile <mo_dir> ([<locale>] | 'all')"
    exit 1
}

for a in "$@"; do
    if [[ $a = '-h' || $a == '--help' ]]; then
        usage_and_exit
    fi
done

if [[ ${1:-} = c* && -n ${2:-} ]]; then
    mo_dir_base="$(realpath -m "$2")"
fi

cd "$(dirname "$0")/../.."

runecho() {
    echo "$ ${@@Q}"
    "$@"
}

each_locale() {
    declare cmd=$1
    shift
    for d in "$po_dir"/*/ ; do
        d=${d:0:-1}  # rtrim /
        $cmd "${d##*/}" "$@"
    done
}

po_dir=tools/i18n/po
trkeys_file=src/translation/trkeys_def.hpp
pot_file="$po_dir/redemption.pot"

case "${1:-}" in

e|extract)
    (( $# == 1 )) || usage_and_exit
    runecho tools/i18n/xgettext.py "$trkeys_file" "$pot_file"
;;

m|merge) # ([<locale>] | 'all')
    merge() {
        runecho msgmerge --update --force-po --no-wrap "$po_dir/$1/redemption.po" "$pot_file"
    }

    if (( $# == 2 )) && [[ $2 == 'all' ]] || (( $# == 1 )) ; then
        each_locale merge
    elif (( $# == 2 )); then
        merge "$2"
    else
        usage_and_exit
    fi
;;

c|compile) # mo_dir ([<locale>] | 'all')
    compile() {
        declare mo_dir="$2/$1/LC_MESSAGES"
        mkdir -p "$mo_dir"
        runecho msgfmt --no-hash --output-file="$mo_dir/redemption.mo" "$po_dir/$1/redemption.po"
    }

    if (( $# == 3 )) && [[ $3 == 'all' ]] || (( $# == 2 )) ; then
        each_locale compile "$mo_dir_base"
    elif (( $# == 3 )); then
        compile "$3" "$mo_dir_base"
    else
        usage_and_exit
    fi
;;

i|init) # locale
    (( $# == 2 )) || usage_and_exit
    declare po="$po_dir/$2"
    mkdir -p "$po"
    runecho msginit --no-translator --no-wrap \
        --locale="$2" --input="$pot_file" --output="$po/redemption.po"
;;

*) usage_and_exit ;;

esac
