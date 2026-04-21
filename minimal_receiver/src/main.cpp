#include "minimal_receiver/TrackerClient.h"

#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
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
        return "vrpn_pose_monitor";
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
    std::cout << "Usage: " << prog << " --tracker <name> [options]\n"
              << "Options:\n"
              << "  --tracker <name>   Tracker name, e.g. sunraynext_uav0\n"
              << "  --host <addr>      VRPN host (default 127.0.0.1)\n"
              << "  --port <port>      VRPN port (default 3883)\n"
              << "  --sample-ms <ms>   Poll interval in milliseconds (default 2)\n"
              << "  --help             Show this help\n\n"
              << "Example:\n"
              << "  " << prog << " --tracker sunraynext_uav0 --host 192.168.10.32 --port 3883\n";
}

std::string normalize_host(std::string host) {
    if (host.empty() || host == "localhost" || host == "::1") {
        return "127.0.0.1";
    }
    return host;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::string tracker_name;
    std::string host = "127.0.0.1";
    int port = 3883;
    int sample_ms = 2;

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
        } else if (arg == "--sample-ms") {
            sample_ms = std::stoi(require_value("--sample-ms"));
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

    if (sample_ms < 1) {
        sample_ms = 1;
    }

    host = normalize_host(host);
    const std::string tracker_address = tracker_name + "@" + host + ":" + std::to_string(port);

    try {
        minimal_receiver::TrackerClient tracker(tracker_address);
        std::optional<minimal_receiver::Pose> last_printed;
        std::uint64_t total_messages = 0;
        std::uint64_t window_messages = 0;
        auto window_start = std::chrono::steady_clock::now();

        while (!g_should_exit) {
            tracker.spin_once();
            if (auto pose = tracker.latest_pose()) {
                const bool changed = !last_printed.has_value() ||
                                     pose->timestamp_sec != last_printed->timestamp_sec;
                if (changed) {
                    ++total_messages;
                    ++window_messages;

                    const auto now = std::chrono::steady_clock::now();
                    const double elapsed = std::chrono::duration<double>(now - window_start).count();
                    const double hz = elapsed > 0.0 ? static_cast<double>(window_messages) / elapsed : 0.0;

                    std::cout.setf(std::ios::fixed);
                    std::cout << std::setprecision(6)
                              << "ts=" << pose->timestamp_sec
                              << " | pos=(" << std::setprecision(4)
                              << pose->x << ", " << pose->y << ", " << pose->z << ")"
                              << " | rpy=(" << pose->roll << ", " << pose->pitch << ", " << pose->yaw << ")"
                              << " | count=" << total_messages
                              << " | hz=" << std::setprecision(2) << hz
                              << '\n';

                    last_printed = pose;
                    if (elapsed >= 1.0) {
                        window_start = now;
                        window_messages = 0;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(sample_ms));
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
