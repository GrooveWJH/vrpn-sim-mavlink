# raw_vrpn_receiver

`raw_vrpn_receiver/` is a first POSIX prototype for reading VRPN Tracker poses from the raw TCP byte stream. It does not include or link against `libvrpn`.

The goal is to learn and isolate the smallest useful VRPN subset before moving the receiver to ESP32 P4/C5. The current subset is intentionally narrow: TCP cookie handshake, sender/type description tables, and `vrpn_Tracker Pos_Quat` pose messages.

## Why This Exists

`minimal_receiver/` is useful on desktop Linux/macOS, but it depends on `vrpn_Tracker_Remote` from the VRPN library. That dependency is too heavy for an embedded migration.

This directory keeps the embedded path visible:

- `socket_io` owns POSIX TCP connect/read/write.
- `vrpn_wire` owns byte order, frame headers, description tables, and `Pos_Quat` payload decode.
- `raw_vrpn_pose_monitor` owns CLI parsing, tracker filtering, pose printing, and rate stats.

For ESP-IDF, the expected move is to replace `socket_io` with an lwIP/BSD sockets adapter and keep most of `vrpn_wire` intact.

## Protocol Subset

The receiver implements only the pieces needed for Tracker pose data:

- Send and receive the 24-byte VRPN cookie.
- Read TCP frames with a 24-byte aligned header.
- Parse system messages:
  - `-1`: sender description
  - `-2`: type description
- Match a frame where:
  - sender name equals `--tracker`
  - type name equals `vrpn_Tracker Pos_Quat`
- Decode the 64-byte pose payload:
  - `int32 sensor`
  - `int32 padding`
  - `double pos[3]`
  - `double quat[4]`

Everything is read in network byte order. Doubles are decoded as big-endian IEEE-754 values without unaligned pointer casts.

## Build

```bash
cd raw_vrpn_receiver
cmake -B build -S .
cmake --build build
```

Run unit tests:

```bash
ctest --test-dir build --output-on-failure
```

## Run Against the SSH Sender

The current test sender is expected at:

```text
sunraynext_sim0@192.168.10.32:3883
```

Run:

```bash
cd raw_vrpn_receiver
./build/raw_vrpn_pose_monitor \
  --tracker sunraynext_sim0 \
  --host 192.168.10.32 \
  --port 3883
```

Limit output for a quick check:

```bash
./build/raw_vrpn_pose_monitor \
  --tracker sunraynext_sim0 \
  --host 192.168.10.32 \
  --port 3883 \
  --max-messages 10
```

Useful debug mode:

```bash
./build/raw_vrpn_pose_monitor \
  --tracker sunraynext_sim0 \
  --host 192.168.10.32 \
  --port 3883 \
  --dump-frames
```

## Local Loopback Test

The loopback script starts the existing VRPN sender, runs this raw receiver, runs `minimal_receiver`, and checks that both print pose data.

```bash
./raw_vrpn_receiver/scripts/test_loopback.sh
```

Generated logs are written under:

```text
raw_vrpn_receiver/build/
```

## CLI

```text
Usage: raw_vrpn_pose_monitor --tracker <name> --host <addr> [options]
Options:
  --tracker <name>      Tracker sender name, e.g. sunraynext_sim0
  --host <addr>         VRPN server host (default 127.0.0.1)
  --port <port>         VRPN server port (default 3883)
  --sample-ms <ms>      Sleep after processed frames (default 2)
  --max-messages <n>    Exit after n matching tracker poses (default unlimited)
  --dump-frames         Print decoded frame sender/type ids to stderr
  --help                Show this help
```

Output:

```text
ts=1777095581.355058 | pos=(-0.0022, -0.3898, 1.0000) | quat=(0.000000, 0.000000, 0.185409, 0.982662) | rpy=(0.0000, 0.0000, 0.3730) | count=1 | hz=113.54
```

## ESP32 P4/C5 Notes

Keep these constraints in mind when moving this prototype:

- ESP32-P4 has no built-in Wi-Fi, so network access must come from Ethernet, an external Wi-Fi path, or a companion such as C5 depending on the board design.
- ESP-IDF exposes lwIP/BSD sockets, so the current POSIX `socket_io` boundary is the right replacement point.
- Avoid dynamic allocation in the hot path. `vrpn_wire` uses fixed-size registry tables and caller-owned buffers.
- Keep the first embedded target to TCP-only Tracker `Pos_Quat`. Add UDP or other VRPN device types only after the pose path is stable.

## Known Limits

- This is not a full VRPN client.
- It skips non-Tracker messages.
- It ignores UDP low-latency setup.
- Registry capacity is fixed at 128 senders and 128 types.
- Payloads larger than the CLI scratch buffer are discarded.
