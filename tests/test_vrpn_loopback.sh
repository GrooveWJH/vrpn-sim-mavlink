#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)
PORT=4000

log() {
  printf '[test] %s\n' "$*"
}

build_component() {
  local dir=$1
  log "Building $dir"
  cmake -B "$dir/build" -S "$dir" >/dev/null
  cmake --build "$dir/build" >/dev/null
}

build_component "$ROOT/Sender"
build_component "$ROOT/Receiver"

log "Starting VRPN sender on port $PORT"
"$ROOT/Sender/build/fake_vrpn_uav_server" --bind ":$PORT" --num-trackers 2 --rate 30 --status-interval 0 --quiet &
SERVER_PID=$!
trap 'kill $SERVER_PID >/dev/null 2>&1 || true' EXIT
sleep 1

RECEIVER_LOG="$ROOT/tests/receiver_test.log"
log "Starting VRPN receiver (UDP output)"
"$ROOT/Receiver/build/vrpn_receiver" \
  --tracker uav0 \
  --host 127.0.0.1 \
  --port $PORT \
  --link udp \
  --udp-target 127.0.0.1:14555 \
  --rate 20 \
  --log-poses >"$RECEIVER_LOG" 2>&1 &
RECEIVER_PID=$!
trap 'kill $RECEIVER_PID >/dev/null 2>&1 || true; kill $SERVER_PID >/dev/null 2>&1 || true' EXIT

log "Letting the system run for 5 seconds"
sleep 5

kill $RECEIVER_PID >/dev/null 2>&1 || true
kill $SERVER_PID >/dev/null 2>&1 || true
trap - EXIT

log "Receiver log saved to $RECEIVER_LOG"
log "Test completed"
