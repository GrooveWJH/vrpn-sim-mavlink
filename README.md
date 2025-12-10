# vrpn-sim-mavlink

![macOS](https://img.shields.io/badge/macOS-supported-success?logo=apple)
![Linux](https://img.shields.io/badge/Linux-supported-success?logo=linux)

Simulate a fleet of VRPN trackers on one computer (Sender) and forward individual tracker poses to a flight controller over MAVLink (Receiver). The initial target is macOS for single-machine testing, but both parts are cross-platform.

```
.
├── Sender     # C++17 VRPN server (fake trackers)
└── Receiver   # Python VRPN → MAVLink bridge
```

## Prerequisites

**macOS**

- Xcode Command Line Tools (`xcode-select --install`)
- CMake ≥ 3.15
- Homebrew packages: `brew install vrpn cmake` (installs VRPN headers/libs for both C++ and Python bindings)
- Python 3.10+ with `pip`

**Linux (Ubuntu / Debian)**

- Build toolchain: `sudo apt install build-essential cmake ninja-build`
- VRPN development package: `sudo apt install libvrpn-dev`
- Python tooling: `sudo apt install python3 python3-venv python3-pip`
- For other distros, install VRPN from source or the package manager and ensure headers/libs are visible to CMake and Python.

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

## 2. Start the VRPN → MAVLink bridge

```bash
cd Receiver
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python vrpn_to_mavlink_bridge.py uav0 127.0.0.1 udpout:127.0.0.1:14550 udp --no-heartbeat
```

- Replace `uav0` with the tracker you want to forward.
- Replace `127.0.0.1` with the VRPN server IP if Sender runs elsewhere.
- Set `mav_endpoint` to a serial device (e.g. `/dev/tty.usbserial-0001` on macOS or `/dev/ttyUSB0` on Linux) and choose `serial` to push data directly to a flight controller, or use `udpout:<host>:<port>` with `udp` for SITL/QGC testing.

The script converts the VRPN quaternions to Euler angles and sends `VISION_POSITION_ESTIMATE` at the requested rate (default 50 Hz).

## 3. Verify in QGroundControl / PX4 / ArduPilot

- For SITL or QGC, open *Widgets → MAVLink Inspector* and watch the `VISION_POSITION_ESTIMATE` message updating.
- For PX4/ArduPilot hardware, enable external vision fusion (e.g. `EKF2_AID_MASK` in PX4) and point the script to the same telemetry/companion link used by the FCU.

## Development notes

- The Sender uses `vrpn_Tracker_Server` mocks with deterministic circular motion so downstream filters receive smooth data.
- The Receiver keeps the most recent pose from VRPN and throttles MAVLink transmission with a configurable loop rate.
- Both components are intentionally dependency-light to make it easy to port them to Linux or Windows hosts.
