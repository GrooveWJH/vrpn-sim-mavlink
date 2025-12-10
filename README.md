# vrpn-sim-mavlink

![macOS](https://img.shields.io/badge/macOS-supported-success?logo=apple)
![Linux](https://img.shields.io/badge/Linux-supported-success?logo=linux)

Simulate a fleet of VRPN trackers on one computer (Sender) and forward individual tracker poses to a flight controller over MAVLink (Receiver). The initial target is macOS for single-machine testing, but both parts are cross-platform.

```
.
├── Sender     # C++17 VRPN server (fake trackers)
└── Receiver   # C++ VRPN → MAVLink bridge (serial/UDP)
```

> **Linux users**: switch to the `linux` branch before building. It carries Linuxbrew/Homebrew-specific tweaks so the Receiver picks up VRPN automatically.

## Prerequisites

**macOS**

- Xcode Command Line Tools (`xcode-select --install`)
- CMake ≥ 3.15
- Homebrew packages: `brew install vrpn cmake`

**Linux (Ubuntu / Debian)**

- Build toolchain: `sudo apt install build-essential cmake ninja-build`
- VRPN development package: `sudo apt install libvrpn-dev`
- If you prefer Linuxbrew/Homebrew, install VRPN there and expose the prefix so CMake can discover the headers/libs:

  ```bash
  eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"  # adjust path to your brew
  brew install vrpn
  export HOMEBREW_PREFIX="$(brew --prefix)"
  export VRPN_ROOT="$HOMEBREW_PREFIX"
  export CMAKE_PREFIX_PATH="$HOMEBREW_PREFIX:$CMAKE_PREFIX_PATH"
  ```
- For other distros, install VRPN from source or the package manager and ensure headers/libs are visible to CMake.

## 1. Build & run the VRPN sender

```bash
cd Sender
cmake -B build -S .
cmake --build build
./build/fake_vrpn_uav_server --bind :3883 --num-trackers 32 --rate 50
```

The simulator publishes trackers `uav0` … `uav31`, all sharing the same connection (`vrpn://<host>:3883`). Press `Ctrl+C` to stop.

- `--bind` accepts strings like `:3883` or `:4000`. VRPN only honors the port number, so any host/IP portion is ignored.
- If another process already listens on `3883` you will see `open_socket: can't bind address`; stop the other process or pick a different `--bind` string (e.g. `:4000`). The server exits on such errors unless you pass `--auto-restart`.
- Use `-q/--quiet` to suppress periodic status logs or `--status-interval` to control their frequency.
- Switch between per-line or inline status updates with `--status-mode append|inline` (default append).
- Status logs include tracker pose/quaternion by default; use `--status-tracker <idx>` to pick which tracker and `--status-no-pose` to disable.
- Pass `--auto-restart --restart-delay 1.5` if you want the sender to keep retrying when connections drop unexpectedly.
- Run `./build/fake_vrpn_uav_server --help` to see all CLI flags plus example invocations.

## 2. Build & run the VRPN → MAVLink bridge (Receiver)

```bash
cd Receiver
cmake -B build -S .
cmake --build build
./build/vrpn_receiver \
    --tracker uav0 \
    --host 127.0.0.1 \
    --port 3883 \
    --link serial \
    --device /dev/tty.usbmodem01 \
    --baud 57600
    --log-poses
```

- Replace `uav0` with the tracker you want to forward.
- Prefer IPv4 addresses for `--host` (`localhost` is normalized to `127.0.0.1`).
- Use `--link udp --udp-target <host>:<port>` to emit via UDP instead of serial.
- Adjust `--sysid/--compid` if your autopilot expects different MAVLink IDs.

The receiver connects to the specified VRPN tracker and transmits `VISION_POSITION_ESTIMATE` at the requested rate (default 50 Hz).

### MAVLink transport to the flight controller

- **Serial/UART (default)** – Specify `--link serial` together with the host serial device exposed by your FCU (for PX4/ArduPilot this is typically a TELEM/COMPANION port cabled to your computer via USB-UART). The binary data are emitted straight onto that UART, so whatever is listening on the other end (PX4 `TELEM2`, ArduPilot `SERIALx`, companion SBC, etc.) receives `VISION_POSITION_ESTIMATE` frames without additional daemons.
- **UDP** – Useful for SITL or QGroundControl; switch to `--link udp --udp-target <ip>:<port>` to broadcast over the network.
- The bridge never waits for MAVLink ACKs; it continuously pushes external-vision poses one-way to the FCU. Make sure your autopilot has vision fusion enabled so it consumes those poses as soon as they arrive on the configured telemetry interface.

## 3. Verify in QGroundControl / PX4 / ArduPilot

- For SITL or QGC, open *Widgets → MAVLink Inspector* and watch the `VISION_POSITION_ESTIMATE` message updating.
- For PX4/ArduPilot hardware, enable external vision fusion (e.g. `EKF2_AID_MASK` in PX4) and point the script to the same telemetry/companion link used by the FCU.

## Development notes

- The Sender uses `vrpn_Tracker_Server` mocks with deterministic circular motion so downstream filters receive smooth data.
- The Receiver is a native C++17 application that links directly against VRPN and the MAVLink C headers for deterministic latency.
- Both components are intentionally dependency-light to make it easy to port them to Linux or Windows hosts.
