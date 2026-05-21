#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)
PORT=${PORT:-4020}
TRACKER=${TRACKER:-sunraynext_sim0}
PREFIX=${PREFIX:-sunraynext_sim}

log() {
  printf '[raw-loopback] %s\n' "$*"
}

build_component() {
  local dir=$1
  log "Building $dir"
  cmake -B "$dir/build" -S "$dir" >/dev/null
  cmake --build "$dir/build" >/dev/null
}

build_component "$ROOT/Sender"
build_component "$ROOT/minimal_receiver"
build_component "$ROOT/raw_vrpn_receiver"

SENDER_LOG="$ROOT/raw_vrpn_receiver/build/sender_loopback.log"
RAW_LOG="$ROOT/raw_vrpn_receiver/build/raw_receiver_loopback.log"
MINIMAL_LOG="$ROOT/raw_vrpn_receiver/build/minimal_receiver_loopback.log"

log "Starting VRPN sender on 127.0.0.1:$PORT as $TRACKER"
"$ROOT/Sender/build/fake_vrpn_uav_server" \
  --bind ":$PORT" \
  --num-trackers 1 \
  --tracker-prefix "$PREFIX" \
  --rate 120 \
  --random-walk \
  --random-radius 1.0 \
  --status-interval 0 \
  --quiet >"$SENDER_LOG" 2>&1 &
SENDER_PID=$!

cleanup() {
  kill "$RAW_PID" >/dev/null 2>&1 || true
  kill "$MINIMAL_PID" >/dev/null 2>&1 || true
  kill "$SENDER_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1

log "Capturing raw receiver output"
"$ROOT/raw_vrpn_receiver/build/raw_vrpn_pose_monitor" \
  --tracker "$TRACKER" \
  --host 127.0.0.1 \
  --port "$PORT" \
  --max-messages 10 \
  --sample-ms 2 >"$RAW_LOG" 2>&1 &
RAW_PID=$!

log "Capturing minimal_receiver output for comparison"
"$ROOT/minimal_receiver/build/vrpn_pose_monitor" \
  --tracker "$TRACKER" \
  --host 127.0.0.1 \
  --port "$PORT" \
  --sample-ms 2 >"$MINIMAL_LOG" 2>&1 &
MINIMAL_PID=$!

sleep 3
kill "$RAW_PID" >/dev/null 2>&1 || true
kill "$MINIMAL_PID" >/dev/null 2>&1 || true
wait "$RAW_PID" || true
wait "$MINIMAL_PID" || true

if ! grep -F 'pos=(' "$RAW_LOG" >/dev/null; then
  log "Raw receiver did not print pose data"
  sed -n '1,80p' "$RAW_LOG"
  exit 1
fi

if ! grep -F 'pos=(' "$MINIMAL_LOG" >/dev/null; then
  log "minimal_receiver did not print pose data"
  sed -n '1,80p' "$MINIMAL_LOG"
  exit 1
fi

log "Raw receiver sample:"
sed -n '1,5p' "$RAW_LOG"
log "minimal_receiver sample:"
sed -n '1,5p' "$MINIMAL_LOG"
log "Loopback comparison passed"
