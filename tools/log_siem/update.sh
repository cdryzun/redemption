#!/bin/sh
cd "$(dirname "$0")"/../..

./tools/log_siem/extractor.py \
  -p -o ./tools/log_siem/siem_filters_rdp_proxy.py \
  -l > ./tools/log_siem/sample.txt
