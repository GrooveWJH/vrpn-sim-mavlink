#include "receiver/MavlinkSender.h"
#include "receiver/TrackerClient.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t g_should_exit = 0;

void handle_signal(int) {
    g_should_exit = 1;
}

const char* program_name(const char* exe) {
    if (exe == nullptr) {
        return "vrpn_receiver";
    }
    const char* slash = std::strrchr(exe, '/');
    const char* backslash = std::strrchr(exe, '\\');
    const char* base = exe;
    if (slash && (!backslash || slash > backslash)) {
        base = slash + 1;
    } else if (backslash) {
        base = backslash + 1;
    }
    return base;
}

void print_usage(const char* exe) {
    const char* prog = program_name(exe);
    std::cout << "Usage: " << prog << " --tracker <name> [options]\n";
    std::cout << "Options:\n"
              << "  --tracker <name>        Tracker name (e.g. uav0)\n"
              << "  --host <addr>           VRPN host (default 127.0.0.1)\n"
              << "  --port <port>           VRPN port (default 3883)\n"
              << "  --rate <Hz>             Publish rate (default 50)\n"
              << "  --link <serial|udp>     Output link type (default serial)\n"
              << "  --device <path>         Serial device (default /dev/ttyUSB0)\n"
              << "  --baud <rate>           Serial baud rate (default 921600)\n"
              << "  --udp-target host:port  UDP target (default 127.0.0.1:14550)\n"
              << "  --sysid <id>            MAVLink system id (default 1)\n"
              << "  --compid <id>           MAVLink component id (default 1)\n"
              << "  --log-poses             Print forwarded poses (default disabled)\n"
              << "\nExamples:\n"
              << "  " << prog
              << " --tracker uav5 --host 192.168.1.50 --port 4000 --rate 40"
                 " --link serial --device /dev/tty.usbmodem01 --baud 57600"
                 " --sysid 1 --compid 196 --log-poses\n"
              << "  " << prog
              << " --tracker uav0 --host 127.0.0.1 --port 3883 --rate 60"
                 " --link udp --udp-target 127.0.0.1:14550 --sysid 42 --compid 200 --log-poses\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::string tracker_name;
    std::string host = "127.0.0.1";
    int port         = 3883;
    double rate_hz   = 50.0;
    receiver::MavlinkOptions link_opts;
    bool log_poses = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << name << " requires a value\n";
                print_usage(argv[0]);
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--tracker") {
            tracker_name = require_value("--tracker");
        } else if (arg == "--host") {
            host = require_value("--host");
        } else if (arg == "--port") {
            port = std::stoi(require_value("--port"));
        } else if (arg == "--rate") {
            rate_hz = std::stod(require_value("--rate"));
        } else if (arg == "--link") {
            link_opts.link_type = require_value("--link");
        } else if (arg == "--device") {
            link_opts.serial_device = require_value("--device");
        } else if (arg == "--baud") {
            link_opts.baud_rate = std::stoi(require_value("--baud"));
        } else if (arg == "--udp-target") {
            link_opts.udp_target = require_value("--udp-target");
        } else if (arg == "--sysid") {
            link_opts.system_id = static_cast<uint8_t>(std::stoi(require_value("--sysid")));
        } else if (arg == "--compid") {
            link_opts.component_id = static_cast<uint8_t>(std::stoi(require_value("--compid")));
        } else if (arg == "--log-poses") {
            log_poses = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (tracker_name.empty()) {
        std::cerr << "--tracker is required\n";
        print_usage(argv[0]);
        return 1;
    }

    if (rate_hz <= 0.0) {
        rate_hz = 50.0;
    }

    auto normalize_host = [](std::string value) {
        if (value == "localhost" || value == "::1" || value.empty()) {
            return std::string("127.0.0.1");
        }
        return value;
    };

    host = normalize_host(host);

    try {
        using clock = std::chrono::steady_clock;
        const auto send_period =
            std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / rate_hz));

        receiver::MavlinkSender sender(link_opts);
        const std::string tracker_address = tracker_name + "@" + host + ":" + std::to_string(port);
        receiver::TrackerClient tracker(tracker_address);

        std::mutex pose_mutex;
        std::optional<receiver::Pose> latest_pose;
        std::atomic<bool> vrpn_running{true};

        std::thread vrpn_thread([&]() {
            while (vrpn_running && !g_should_exit) {
                if (!tracker.spin_once()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                if (auto pose = tracker.latest_pose()) {
                    std::lock_guard<std::mutex> lock(pose_mutex);
                    latest_pose = *pose;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });

        auto next_send = clock::now();
        while (!g_should_exit) {
            const auto now = clock::now();
            if (now >= next_send) {
                std::optional<receiver::Pose> pose_copy;
                {
                    std::lock_guard<std::mutex> lock(pose_mutex);
                    pose_copy = latest_pose;
                }
                if (pose_copy) {
                    sender.send_pose(*pose_copy);
                    if (log_poses) {
                        std::cout.setf(std::ios::fixed);
                        std::cout.precision(3);
                        std::cout << "[vrpn_receiver] t=" << pose_copy->timestamp_sec << " pos=("
                                  << std::setw(7) << pose_copy->x << ", " << std::setw(7)
                                  << pose_copy->y << ", " << std::setw(4) << pose_copy->z << ") rpy=("
                                  << std::setw(6) << pose_copy->roll << ", " << std::setw(6)
                                  << pose_copy->pitch << ", " << std::setw(7) << pose_copy->yaw
                                  << ")\n";
                    }
                }
                do {
                    next_send += send_period;
                } while (now >= next_send);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        vrpn_running = false;
        if (vrpn_thread.joinable()) {
            vrpn_thread.join();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
