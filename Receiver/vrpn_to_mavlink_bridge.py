#!/usr/bin/env python3
"""Bridge VRPN tracker poses into MAVLink VISION_POSITION_ESTIMATE messages."""

import argparse
import math
import threading
import time
from typing import Optional, Tuple

import vrpn  # type: ignore
from pymavlink import mavutil

Pose = Tuple[float, float, float, float, float, float, float]


class VrpnTrackerClient:
    """Store the latest pose coming from a VRPN tracker stream."""

    def __init__(self, tracker_address: str):
        self._tracker = vrpn.receiver.Tracker(tracker_address)
        self._lock = threading.Lock()
        self._latest_pose: Optional[Pose] = None
        self._tracker.register_change_handler(None, self._handle_tracker, "position")

    def _handle_tracker(self, _userdata, data):
        pos = data.get("position", (0.0, 0.0, 0.0))
        quat = data.get("quaternion", (0.0, 0.0, 0.0, 1.0))
        t_sec = data.get("time", time.time())

        roll, pitch, yaw = quaternion_to_euler(*quat)
        pose: Pose = (t_sec, pos[0], pos[1], pos[2], roll, pitch, yaw)
        with self._lock:
            self._latest_pose = pose

    def spin_once(self):
        self._tracker.mainloop()

    def latest_pose(self) -> Optional[Pose]:
        with self._lock:
            return self._latest_pose


def quaternion_to_euler(qx: float, qy: float, qz: float, qw: float):
    sinr_cosp = 2.0 * (qw * qx + qy * qz)
    cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy)
    roll = math.atan2(sinr_cosp, cosr_cosp)

    sinp = 2.0 * (qw * qy - qz * qx)
    if abs(sinp) >= 1:
        pitch = math.copysign(math.pi / 2, sinp)
    else:
        pitch = math.asin(sinp)

    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    yaw = math.atan2(siny_cosp, cosy_cosp)
    return roll, pitch, yaw


class MavlinkSender:
    def __init__(self, endpoint: str, baudrate: int, use_udp: bool, wait_heartbeat: bool = True):
        if use_udp:
            self._master = mavutil.mavlink_connection(endpoint)
        else:
            self._master = mavutil.mavlink_connection(endpoint, baud=baudrate)

        if wait_heartbeat:
            print("Waiting for MAVLink heartbeat...")
            self._master.wait_heartbeat()
            print(
                "Heartbeat received from system",
                self._master.target_system,
                "component",
                self._master.target_component,
            )

    def send_vision_position_estimate(self, pose: Pose):
        t_sec, x, y, z, roll, pitch, yaw = pose
        usec = int(t_sec * 1e6)
        covariance = [0.0] * 21
        self._master.mav.vision_position_estimate_send(
            usec,
            x,
            y,
            z,
            roll,
            pitch,
            yaw,
            covariance,
        )


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tracker", help="Tracker name, e.g. uav0")
    parser.add_argument("vrpn_host", help="VRPN server IP/hostname")
    parser.add_argument("mav_endpoint", help="Serial device or UDP descriptor")
    parser.add_argument(
        "link_type",
        choices=["serial", "udp"],
        help="serial -> open mav_endpoint with baud rate, udp -> pass to pymavlink as-is",
    )
    parser.add_argument("--vrpn-port", type=int, default=3883, dest="vrpn_port")
    parser.add_argument("--publish-rate", type=float, default=50.0, dest="publish_rate")
    parser.add_argument("--baud", type=int, default=921600, dest="baudrate")
    parser.add_argument(
        "--no-heartbeat",
        action="store_true",
        help="Do not wait for MAVLink heartbeat (useful for SITL where messages can be sent immediately)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Connect to VRPN but skip MAVLink transmission (for debugging)",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    tracker_address = f"{args.tracker}@{args.vrpn_host}:{args.vrpn_port}"
    print(f"Connecting to VRPN tracker {tracker_address}")
    tracker = VrpnTrackerClient(tracker_address)

    sender = None
    if not args.dry_run:
        sender = MavlinkSender(
            endpoint=args.mav_endpoint,
            baudrate=args.baudrate,
            use_udp=(args.link_type == "udp"),
            wait_heartbeat=(not args.no_heartbeat),
        )
    else:
        print("Dry run mode: not opening MAVLink connection")

    dt = 1.0 / args.publish_rate
    try:
        while True:
            loop_start = time.time()
            tracker.spin_once()
            pose = tracker.latest_pose()
            if pose and sender:
                sender.send_vision_position_estimate(pose)
            elif pose and not sender:
                print(
                    f"Pose @ {pose[0]:.3f}s ->"
                    f" xyz=({pose[1]:.2f}, {pose[2]:.2f}, {pose[3]:.2f})"
                    f" rpy=({pose[4]:.2f}, {pose[5]:.2f}, {pose[6]:.2f})"
                )
            elapsed = time.time() - loop_start
            sleep_time = dt - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
    except KeyboardInterrupt:
        print("Stopping VRPN → MAVLink bridge")


if __name__ == "__main__":
    main()
