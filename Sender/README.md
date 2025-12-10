# Sender (VRPN server)

This directory hosts a tiny C++17 VRPN server that simulates up to 32 trackers (`uav0`..`uav31`).

## Build (macOS / Linux)

1. Install VRPN.
   - macOS: `brew install vrpn cmake`
   - Ubuntu/Debian: `sudo apt install build-essential cmake libvrpn-dev`
   - Other Linux distributions: install the VRPN headers/libs via the distro package manager or from source.
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
| `-b`, `--bind <str>` | VRPN bind string, defaults to `:3883`. Specify `:PORT` to choose a different TCP port. |
| `-n`, `--num-trackers <N>` | Number of trackers to spawn (default 32). |
| `-r`, `--rate <Hz>` | Publish rate in Hz (default 50). |
| `-q`, `--quiet` | Suppress periodic status logs (still prints critical errors). |
| `--status-interval <s>` | Seconds between status messages (default 5s, set ≤0 to disable). |
| `--status-mode <mode>` | `append` (default) prints a new line each time, `inline` rewrites the same console line. |
| `--status-tracker <idx>` | Tracker index whose pose appears in the status logs (default 0). |
| `--status-no-pose` | Disable printing pose/quaternion data in status logs (enabled by default). |
| `--auto-restart` | Automatically tear down and rebind when the VRPN connection errors out. |
| `--restart-delay <s>` | Delay before attempting to restart (default 1s). |

By default the server exits when it encounters a VRPN connection error (for example, when the port is already in use). Combine `--auto-restart` with `--restart-delay` to keep trying until the socket becomes available again.

> VRPN only honors the *port* embedded in `--bind`. If you pass strings like `0.0.0.0:3883` or `vrpn:0.0.0.0:3883`, the server automatically normalizes them to `:3883`, meaning it listens on all interfaces.

Each tracker reports a simple circular trajectory plus yaw rotation, so any VRPN client can subscribe to `uavX@<ip>:3883` and receive pose updates.

Run `./build/fake_vrpn_uav_server --help` to view the full list of flags plus example command lines demonstrating typical configurations.
