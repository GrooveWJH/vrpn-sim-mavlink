#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>

#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

namespace {
volatile std::sig_atomic_t g_should_exit = 0;

void handle_signal(int) {
    g_should_exit = 1;
}

struct ProgramOptions {
    std::string bind_address = ":3883";   // matches vrpn notation
    int tracker_count        = 32;
    double publish_rate_hz   = 50.0;
};

ProgramOptions parse_args(int argc, char** argv) {
    ProgramOptions opts;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if ((std::strcmp(arg, "-b") == 0 || std::strcmp(arg, "--bind") == 0) &&
            i + 1 < argc) {
            opts.bind_address = argv[++i];
        } else if ((std::strcmp(arg, "-n") == 0 || std::strcmp(arg, "--num-trackers") == 0) &&
                   i + 1 < argc) {
            opts.tracker_count = std::atoi(argv[++i]);
        } else if ((std::strcmp(arg, "-r") == 0 || std::strcmp(arg, "--rate") == 0) &&
                   i + 1 < argc) {
            opts.publish_rate_hz = std::atof(argv[++i]);
        } else if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            std::printf("Usage: %s [options]\n", argv[0]);
            std::printf("  -b, --bind <addr>          VRPN bind string (default :3883)\n");
            std::printf("  -n, --num-trackers <N>     Number of trackers to simulate (default 32)\n");
            std::printf("  -r, --rate <Hz>            Publish rate (default 50Hz)\n");
            std::printf("Example: %s --bind vrpn:0.0.0.0:3883 --num-trackers 16\n", argv[0]);
            std::exit(0);
        }
    }
    return opts;
}

inline timeval now_timeval() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto us  = duration_cast<microseconds>(now.time_since_epoch()).count();
    timeval tv{};
    tv.tv_sec  = static_cast<long>(us / 1000000);
    tv.tv_usec = static_cast<long>(us % 1000000);
    return tv;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    ProgramOptions opts = parse_args(argc, argv);
    if (opts.tracker_count <= 0) {
        std::fprintf(stderr, "Tracker count must be > 0\n");
        return 1;
    }
    if (opts.publish_rate_hz <= 0.0) {
        std::fprintf(stderr, "Publish rate must be > 0\n");
        return 1;
    }

    vrpn_Connection* connection = vrpn_create_server_connection(opts.bind_address.c_str());
    if (!connection) {
        std::fprintf(stderr, "Failed to bind VRPN server on %s\n", opts.bind_address.c_str());
        return 1;
    }

    std::printf("VRPN server listening on %s\n", opts.bind_address.c_str());

    std::vector<std::unique_ptr<vrpn_Tracker_Server>> trackers;
    trackers.reserve(opts.tracker_count);

    for (int i = 0; i < opts.tracker_count; ++i) {
        char tracker_name[32];
        std::snprintf(tracker_name, sizeof(tracker_name), "uav%d", i);
        trackers.emplace_back(std::make_unique<vrpn_Tracker_Server>(tracker_name, connection, 1));
        std::printf("  spawned tracker %s\n", tracker_name);
    }

    const double dt = 1.0 / opts.publish_rate_hz;
    double sim_time = 0.0;

    while (!g_should_exit) {
        timeval ts = now_timeval();

        for (int i = 0; i < opts.tracker_count; ++i) {
            auto* tracker = trackers[i].get();

            const double radius = 2.0 + 0.1 * i;
            const double omega  = 0.2 + 0.01 * i;
            const double phase  = i * (M_PI / 16.0);

            const double angle = omega * sim_time + phase;

            vrpn_float64 pos[3];
            pos[0] = radius * std::cos(angle);
            pos[1] = radius * std::sin(angle);
            pos[2] = 1.0 + 0.05 * i;

            const double yaw      = angle;
            const double half_yaw = yaw * 0.5;
            vrpn_float64 quat[4];
            quat[0] = 0.0;
            quat[1] = 0.0;
            quat[2] = std::sin(half_yaw);
            quat[3] = std::cos(half_yaw);

            tracker->report_pose(0, ts, pos, quat);
        }

        connection->mainloop();
        std::this_thread::sleep_for(std::chrono::duration<double>(dt));
        sim_time += dt;
    }

    std::printf("Shutting down VRPN server...\n");

    for (auto& tracker : trackers) {
        tracker.reset();
    }
    connection->removeReference();
    return 0;
}
