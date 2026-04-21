#include "vrpn_sim/FakeTrackerServer.h"

#include "vrpn_sim/ProgramOptions.h"

#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
std::atomic<bool> g_should_exit{false};

void handle_signal(int) {
    g_should_exit.store(true);
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

inline double unix_time_seconds() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto duration_since_epoch = now.time_since_epoch();
    return duration_cast<duration<double>>(duration_since_epoch).count();
}

}  // namespace

namespace vrpn_sim {

void install_signal_handlers() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
}

struct FakeTrackerServer::Impl {
    struct TrackerSample {
        vrpn_float64 pos[3]  = {0.0, 0.0, 0.0};
        vrpn_float64 quat[4] = {0.0, 0.0, 0.0, 1.0};
    };

    struct RandomTrackerState {
        double x = 0.0;
        double y = 0.0;
        double prev_x = 0.0;
        double prev_y = 0.0;
        double target_x = 0.0;
        double target_y = 0.0;
    };

    explicit Impl(ProgramOptions options)
        : opts_(std::move(options)), dt_(1.0 / opts_.publish_rate_hz), rng_(std::random_device{}()) {
        if (opts_.tracker_count <= 0) {
            std::fprintf(stderr, "Tracker count must be > 0\n");
            throw std::runtime_error("invalid tracker count");
        }
        if (opts_.publish_rate_hz <= 0.0) {
            std::fprintf(stderr, "Publish rate must be > 0\n");
            throw std::runtime_error("invalid publish rate");
        }
        if (opts_.status_interval_s < 0.0) {
            opts_.status_interval_s = 0.0;
        }
        if (opts_.restart_delay_s < 0.0) {
            opts_.restart_delay_s = 0.0;
        }
        if (opts_.status_pose_tracker < 0) {
            opts_.status_pose_tracker = 0;
        }
        if (opts_.random_radius_m <= 0.0) {
            opts_.random_radius_m = 1.0;
        }
        if (opts_.tracker_prefix.empty()) {
            opts_.tracker_prefix = "uav";
        }
        normalize_bind_address();
        tracker_samples_.resize(opts_.tracker_count);
        random_states_.resize(opts_.tracker_count);
        for (auto& state : random_states_) {
            auto [x, y] = sample_point_in_disc(opts_.random_radius_m);
            state.target_x = x;
            state.target_y = y;
        }
    }

    int run() {
        try {
            while (!g_should_exit.load()) {
                if (!create_connection()) {
                    return 1;
                }
                spawn_trackers();
                mainloop();
                teardown_connection();

                if (g_should_exit.load()) {
                    break;
                }

                if (!opts_.auto_restart || !connection_failed_) {
                    if (connection_failed_ && !opts_.auto_restart) {
                        std::fprintf(stderr, "VRPN server exiting after a connection failure. Pass --auto-restart to keep retrying.\n");
                    }
                    break;
                }

                log_info("Restarting VRPN server in %.1fs...", opts_.restart_delay_s);
                std::this_thread::sleep_for(std::chrono::duration<double>(opts_.restart_delay_s));
                sim_time_ = 0.0;
                connection_failed_ = false;
            }
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "Fatal error: %s\n", ex.what());
            return 1;
        }

        if (opts_.status_single_line) {
            std::printf("\n");
        }
        return 0;
    }

private:
    std::pair<double, double> sample_point_in_disc(double radius) {
        std::uniform_real_distribution<double> dist(-radius, radius);
        while (true) {
            const double x = dist(rng_);
            const double y = dist(rng_);
            if (x * x + y * y <= radius * radius) {
                return {x, y};
            }
        }
    }

    void normalize_bind_address() {
        if (opts_.bind_address.empty()) {
            opts_.bind_address = ":3883";
            return;
        }
        if (opts_.bind_address.front() == ':' || opts_.bind_address.find("://") != std::string::npos) {
            if (opts_.bind_address.front() == ':') {
                return;
            }
        }

        std::string trimmed = opts_.bind_address;
        const std::string prefix = "vrpn:";
        if (trimmed.rfind(prefix, 0) == 0) {
            trimmed = trimmed.substr(prefix.size());
        }
        auto colon = trimmed.rfind(':');
        if (colon == std::string::npos || colon + 1 >= trimmed.size()) {
            return;
        }
        std::string port = trimmed.substr(colon + 1);
        if (port.empty()) {
            return;
        }
        auto digits = std::all_of(port.begin(), port.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        });
        if (!digits) {
            return;
        }
        std::fprintf(stderr,
                     "Normalizing bind string '%s' -> ':%s'. VRPN only honors the port component.\n",
                     opts_.bind_address.c_str(),
                     port.c_str());
        opts_.bind_address = ":" + port;
    }

    bool create_connection() {
        connection_ = vrpn_create_server_connection(opts_.bind_address.c_str());
        if (!connection_) {
            std::fprintf(stderr, "Failed to bind VRPN server on %s\n", opts_.bind_address.c_str());
            return false;
        }
        log_info("VRPN server listening on %s", opts_.bind_address.c_str());
        return true;
    }

    void spawn_trackers() {
        trackers_.clear();
        trackers_.reserve(opts_.tracker_count);
        for (int i = 0; i < opts_.tracker_count; ++i) {
            char tracker_name[128];
            std::snprintf(tracker_name, sizeof(tracker_name), "%s%d", opts_.tracker_prefix.c_str(), i);
            trackers_.emplace_back(std::make_unique<vrpn_Tracker_Server>(tracker_name, connection_, 1));
            log_info("  spawned tracker %s", tracker_name);
        }
    }

    void mainloop() {
        const double dt = 1.0 / opts_.publish_rate_hz;
        double last_status_time = -opts_.status_interval_s;
        connection_failed_     = false;

        while (!g_should_exit.load()) {
            if (!connection_->doing_okay()) {
                std::fprintf(stderr, "VRPN connection reported an error.\n");
                connection_failed_ = true;
                break;
            }

            timeval ts = now_timeval();
            publish_trackers(ts);
            connection_->mainloop();
            std::this_thread::sleep_for(std::chrono::duration<double>(dt));
            sim_time_ += dt;

            if (opts_.status_interval_s > 0.0 && sim_time_ - last_status_time >= opts_.status_interval_s) {
                write_status_line();
                last_status_time = sim_time_;
            }
        }
    }

    void update_random_tracker(int i, vrpn_float64 pos[3], vrpn_float64 quat[4]) {
        auto& state = random_states_[i];
        const double radius = opts_.random_radius_m;
        const double max_step = std::max(0.01, radius * 0.04);

        double dx = state.target_x - state.x;
        double dy = state.target_y - state.y;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist < max_step) {
            state.prev_x = state.x;
            state.prev_y = state.y;
            auto [nx, ny] = sample_point_in_disc(radius);
            state.target_x = nx;
            state.target_y = ny;
            dx = state.target_x - state.x;
            dy = state.target_y - state.y;
            dist = std::sqrt(dx * dx + dy * dy);
        }

        if (dist > 1e-9) {
            const double step = std::min(max_step, dist);
            state.prev_x = state.x;
            state.prev_y = state.y;
            state.x += dx / dist * step;
            state.y += dy / dist * step;
        }

        pos[0] = state.x;
        pos[1] = state.y;
        pos[2] = 1.0;

        const double move_dx = state.x - state.prev_x;
        const double move_dy = state.y - state.prev_y;
        const double yaw = (std::abs(move_dx) < 1e-9 && std::abs(move_dy) < 1e-9) ? 0.0 : std::atan2(move_dy, move_dx);
        const double half_yaw = yaw * 0.5;
        quat[0] = 0.0;
        quat[1] = 0.0;
        quat[2] = std::sin(half_yaw);
        quat[3] = std::cos(half_yaw);
    }

    void publish_trackers(const timeval& ts) {
        for (int i = 0; i < opts_.tracker_count; ++i) {
            auto* tracker = trackers_[i].get();
            vrpn_float64 pos[3];
            vrpn_float64 quat[4];

            if (opts_.random_walk) {
                update_random_tracker(i, pos, quat);
            } else {
                const double radius = 2.0 + 0.1 * i;
                const double omega  = 0.2 + 0.01 * i;
                const double phase  = i * (M_PI / 16.0);
                const double angle  = omega * sim_time_ + phase;

                pos[0] = radius * std::cos(angle);
                pos[1] = radius * std::sin(angle);
                pos[2] = 1.0 + 0.05 * i;

                const double half_yaw = angle * 0.5;
                quat[0] = 0.0;
                quat[1] = 0.0;
                quat[2] = std::sin(half_yaw);
                quat[3] = std::cos(half_yaw);
            }

            tracker->report_pose(0, ts, pos, quat);
            tracker_samples_[i].pos[0]  = pos[0];
            tracker_samples_[i].pos[1]  = pos[1];
            tracker_samples_[i].pos[2]  = pos[2];
            tracker_samples_[i].quat[0] = quat[0];
            tracker_samples_[i].quat[1] = quat[1];
            tracker_samples_[i].quat[2] = quat[2];
            tracker_samples_[i].quat[3] = quat[3];
        }
    }

    void write_status_line() {
        if (opts_.quiet) {
            return;
        }
        const double unix_ts = unix_time_seconds();
        if (opts_.status_include_pose && !tracker_samples_.empty()) {
            const int tracker_idx = std::clamp(opts_.status_pose_tracker, 0, opts_.tracker_count - 1);
            const auto& sample = tracker_samples_[tracker_idx];
            print_status(
                "[%.3f] Sim time %.2fs | trackers: %d | interval %.1fs | tracker%d pos=(%.2f, %.2f, %.2f) quat=(%.3f, %.3f, %.3f, %.3f)",
                unix_ts,
                sim_time_,
                opts_.tracker_count,
                opts_.status_interval_s,
                tracker_idx,
                sample.pos[0],
                sample.pos[1],
                sample.pos[2],
                sample.quat[0],
                sample.quat[1],
                sample.quat[2],
                sample.quat[3]);
        } else {
            print_status("[%.3f] Sim time %.2fs | trackers: %d | interval %.1fs",
                         unix_ts,
                         sim_time_,
                         opts_.tracker_count,
                         opts_.status_interval_s);
        }
    }

    void print_status(const char* fmt, ...) const {
        char buffer[256];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        if (opts_.status_single_line) {
            std::printf("\r%s", buffer);
            std::fflush(stdout);
        } else {
            std::printf("%s\n", buffer);
        }
    }

    void log_info(const char* fmt, ...) const {
        if (opts_.quiet) {
            return;
        }
        va_list args;
        va_start(args, fmt);
        std::vprintf(fmt, args);
        std::printf("\n");
        va_end(args);
    }

    void teardown_connection() {
        trackers_.clear();
        if (connection_) {
            if (!opts_.quiet) {
                std::printf("Shutting down VRPN server...\n");
            }
            connection_->removeReference();
            connection_ = nullptr;
        }
    }

    ProgramOptions opts_;
    double dt_ = 0.0;
    double sim_time_ = 0.0;
    vrpn_Connection* connection_ = nullptr;
    std::vector<std::unique_ptr<vrpn_Tracker_Server>> trackers_;
    std::vector<TrackerSample> tracker_samples_;
    std::vector<RandomTrackerState> random_states_;
    std::mt19937 rng_;
    bool connection_failed_ = false;
};

FakeTrackerServer::FakeTrackerServer(ProgramOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

FakeTrackerServer::~FakeTrackerServer() = default;

int FakeTrackerServer::run() {
    return impl_->run();
}

}  // namespace vrpn_sim
