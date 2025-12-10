# vrpn-sim-mavlink

Simulate a fleet of VRPN trackers on one computer (Sender) and forward individual tracker poses to a flight controller over MAVLink (Receiver). The initial target is macOS for single-machine testing, but both parts are cross-platform.

```
.
├── Sender     # C++17 VRPN server (fake trackers)
└── Receiver   # Python VRPN → MAVLink bridge
```

## Prerequisites (macOS)

- Xcode Command Line Tools (`xcode-select --install`)
- CMake ≥ 3.15
- Homebrew packages: `brew install vrpn cmake` (installs VRPN headers/libs for both C++ and Python bindings)
- Python 3.10+ with `pip`

## 1. Build & run the VRPN sender

```bash
cd Sender
cmake -B build -S .
cmake --build build
./build/fake_vrpn_uav_server --bind vrpn:0.0.0.0:3883 --num-trackers 32 --rate 50
```

The simulator publishes trackers `uav0` … `uav31`, all sharing the same connection (`vrpn://<host>:3883`). Press `Ctrl+C` to stop.

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
- Set `mav_endpoint` to a serial device (e.g. `/dev/tty.usbserial-0001`) and choose `serial` to push data directly to a flight controller, or use `udpout:<host>:<port>` with `udp` for SITL/QGC testing.

The script converts the VRPN quaternions to Euler angles and sends `VISION_POSITION_ESTIMATE` at the requested rate (default 50 Hz).

## 3. Verify in QGroundControl / PX4 / ArduPilot

- For SITL or QGC, open *Widgets → MAVLink Inspector* and watch the `VISION_POSITION_ESTIMATE` message updating.
- For PX4/ArduPilot hardware, enable external vision fusion (e.g. `EKF2_AID_MASK` in PX4) and point the script to the same telemetry/companion link used by the FCU.

## Development notes

- The Sender uses `vrpn_Tracker_Server` mocks with deterministic circular motion so downstream filters receive smooth data.
- The Receiver keeps the most recent pose from VRPN and throttles MAVLink transmission with a configurable loop rate.
- Both components are intentionally dependency-light to make it easy to port them to Linux or Windows hosts.
