# Receiver (C++ VRPN → MAVLink bridge)

This directory now contains a C++17 application that listens to a VRPN tracker and republishes poses as MAVLink `VISION_POSITION_ESTIMATE` messages over either a serial device or UDP socket.

## Build

```bash
cd Receiver
cmake -B build -S .
cmake --build build
```

Requirements:

- VRPN headers/libraries available on your system (`brew install vrpn` or `sudo apt install libvrpn-dev`).
- A C++17 compiler and CMake ≥ 3.15.

## Usage (CLI reference)

Flags:

- `--tracker`: tracker name (`uav0`, …)
- `--host`, `--port`: VRPN server location (IPv4 preferred; `localhost` is automatically mapped to `127.0.0.1`)
- `--rate`: send frequency (Hz, default 50)
- `--link`: `serial` (default) or `udp`
- `--device`, `--baud`: serial configuration
- `--udp-target`: `<host>:<port>`
- `--sysid`, `--compid`: MAVLink IDs
- `--log-poses`: print each forwarded pose to stdout (useful for debugging/log capture)

### Example commands

**Full serial pipeline (uses every serial-related arg)**

```bash
./build/vrpn_receiver \
    --tracker uav5 \
    --host 192.168.1.50 \
    --port 4000 \
    --rate 40 \
    --link serial \
    --device /dev/tty.usbmodem01 \
    --baud 57600 \
    --sysid 1 \
    --compid 196 \
    --log-poses
```

This connects to `uav5@192.168.1.50:4000`, forwards poses at 40 Hz over `/dev/tty.usbmodem01`, and tags each MAVLink packet with `(sysid=1, compid=196)` while echoing poses to stdout for inspection.

**UDP bridge with custom IDs**

```bash
./build/vrpn_receiver \
    --tracker uav0 \
    --host 127.0.0.1 \
    --port 3883 \
    --rate 60 \
    --link udp \
    --udp-target 127.0.0.1:14550 \
    --sysid 42 \
    --compid 200 \
    --log-poses
```

The above sends a 60 Hz stream to `udpout:127.0.0.1:14550` (ideal for QGC/SITL) while exercising every UDP option plus the diagnostic pose logging flag.

### Which MAVLink interface is used?

- **Serial/UART (default):** The tool writes MAVLink bytes directly to the device passed via `--device`. On macOS a PX4/ArduPilot board that is plugged in over USB typically appears as `/dev/tty.usbmodemXX` (CDC ACM) or `/dev/tty.usbserial-XXXX`. Hardware-wise, that port is bridged to the autopilot’s TELEM/COMPANION UART, so the FCU immediately consumes the `VISION_POSITION_ESTIMATE` stream just as if it came from any companion computer.
- **UDP:** For SITL or desktop testing you can switch to `--link udp` and target `udpout:<ip>:<port>` (e.g. `127.0.0.1:14550` for QGroundControl). This does not require any physical wiring and is useful when no FCU is attached.

- The bridge sends poses one-way; MAVLink acknowledgements are not required. Configure your autopilot to fuse external vision (for PX4 set `EKF2_AID_MASK` appropriately; for ArduPilot enable `VISUAL_POSITION` aids) and leave the bridge running so it continuously feeds poses over the telemetry port.

Press `Ctrl+C` to exit.
