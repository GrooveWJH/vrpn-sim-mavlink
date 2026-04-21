#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)
TARGET_DIR="$ROOT/minimal_receiver"

log() {
  printf '[test] %s\n' "$*"
}

if [[ ! -d "$TARGET_DIR" ]]; then
  log "Missing directory: $TARGET_DIR"
  exit 1
fi

BUILD_DIR="$TARGET_DIR/build"

log "Configuring minimal_receiver"
cmake -B "$BUILD_DIR" -S "$TARGET_DIR" >/dev/null

log "Building vrpn_pose_monitor"
cmake --build "$BUILD_DIR" >/dev/null

log "Checking help output"
HELP_OUTPUT=$("$BUILD_DIR/vrpn_pose_monitor" --help)
printf '%s\n' "$HELP_OUTPUT" | grep -F -- "--tracker <name>" >/dev/null
printf '%s\n' "$HELP_OUTPUT" | grep -F -- "--sample-ms <ms>" >/dev/null

log "minimal_receiver smoke test passed"
