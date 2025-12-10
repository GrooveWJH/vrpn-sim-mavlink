# Sender (VRPN server)

This directory hosts a tiny C++17 VRPN server that simulates up to 32 trackers (`uav0`..`uav31`).

## Build (macOS / Linux)

1. Install VRPN. On macOS: `brew install vrpn`. On Linux build the official sources and install into `/usr/local`.
2. Configure and build:

```bash
cd Sender
cmake -B build -S .
cmake --build build
```

The resulting binary is `build/fake_vrpn_uav_server`.

## Run

```
./build/fake_vrpn_uav_server [options]
```

Options:

| flag | meaning |
| ---- | ------- |
| `-b`, `--bind <str>` | VRPN bind string, defaults to `:3883`. Use `vrpn:0.0.0.0:3883` to listen on all interfaces. |
| `-n`, `--num-trackers <N>` | Number of trackers to spawn (default 32). |
| `-r`, `--rate <Hz>` | Publish rate in Hz (default 50). |

Each tracker reports a simple circular trajectory plus yaw rotation, so any VRPN client can subscribe to `uavX@<ip>:3883` and receive pose updates.
