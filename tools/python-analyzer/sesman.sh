#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"/../..

sesman_file=tools/sesman/sesmanworker/sesman.py

s=$(grep ^DEBUG "$sesman_file")

if [[ "$s" != 'DEBUG = False' ]];
then
    echo "Error: $sesman_file: DEBUG is enabled !!!" >&2
    exit 1
fi
