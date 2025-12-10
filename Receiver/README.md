# Receiver (VRPN → MAVLink bridge)

This Python utility connects to a VRPN tracker (e.g. `uav0@localhost:3883`) and forwards the pose as `VISION_POSITION_ESTIMATE` to any MAVLink endpoint (serial or UDP).

## Setup

```bash
cd Receiver
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

> The `vrpn` Python module is a thin wrapper over the VRPN C library. On macOS install VRPN via `brew install vrpn` before running `pip install vrpn`.

## Usage

Run the sender on the same Mac (`./Sender/build/fake_vrpn_uav_server`). Then start the bridge:

```bash
python vrpn_to_mavlink_bridge.py \
    uav0 127.0.0.1 /dev/tty.usbserial-0001 serial
```

UDP example for SITL / QGC on the same host:

```bash
python vrpn_to_mavlink_bridge.py \
    uav0 127.0.0.1 udpout:127.0.0.1:14550 udp --no-heartbeat
```

Flags of interest:

- `--vrpn-port`: change VRPN port (default 3883)
- `--publish-rate`: change the MAVLink send rate (default 50 Hz)
- `--baud`: set serial baud rate (default 921600)
- `--dry-run`: print VRPN poses without touching MAVLink
