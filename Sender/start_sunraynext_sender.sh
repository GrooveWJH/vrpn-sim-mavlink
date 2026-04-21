#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
exec ./build/fake_vrpn_uav_server \
  --bind :3883 \
  --num-trackers 1 \
  --tracker-prefix sunraynext_uav \
  --rate 120 \
  --random-walk \
  --random-radius 1.0 \
  --status-mode inline \
  --status-interval 1
