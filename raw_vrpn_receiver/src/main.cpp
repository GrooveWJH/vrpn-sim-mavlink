#include "raw_vrpn_receiver/socket_io.h"
#include "raw_vrpn_receiver/vrpn_wire.h"

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

volatile std::sig_atomic_t g_should_exit = 0;

struct Options {
    const char* tracker = nullptr;
    const char* host = "127.0.0.1";
    int port = 3883;
    int sample_ms = 2;
    int max_messages = 0;
    bool dump_frames = false;
};

struct Rpy {
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
};

void handle_signal(int) {
    g_should_exit = 1;
}

const char* program_name(const char* exe) {
    if (exe == nullptr) {
        return "raw_vrpn_pose_monitor";
    }
    const char* slash = std::strrchr(exe, '/');
    return slash == nullptr ? exe : slash + 1;
}

void print_usage(const char* exe) {
    const char* prog = program_name(exe);
    std::printf("Usage: %s --tracker <name> --host <addr> [options]\n", prog);
    std::printf("Options:\n");
    std::printf("  --tracker <name>      Tracker sender name, e.g. sunraynext_sim0\n");
    std::printf("  --host <addr>         VRPN server host (default 127.0.0.1)\n");
    std::printf("  --port <port>         VRPN server port (default 3883)\n");
    std::printf("  --sample-ms <ms>      Sleep after processed frames (default 2)\n");
    std::printf("  --max-messages <n>    Exit after n matching tracker poses (default unlimited)\n");
    std::printf("  --dump-frames         Print decoded frame sender/type ids to stderr\n");
    std::printf("  --help                Show this help\n");
}

bool parse_int(const char* text, int* out) {
    if (text == nullptr || out == nullptr) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool parse_args(int argc, char** argv, Options* out) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a value\n", name);
                return nullptr;
            }
            return argv[++i];
        };

        if (std::strcmp(arg, "--tracker") == 0) {
            out->tracker = require_value("--tracker");
            if (out->tracker == nullptr) {
                return false;
            }
        } else if (std::strcmp(arg, "--host") == 0) {
            out->host = require_value("--host");
            if (out->host == nullptr) {
                return false;
            }
        } else if (std::strcmp(arg, "--port") == 0) {
            const char* value = require_value("--port");
            if (value == nullptr || !parse_int(value, &out->port)) {
                std::fprintf(stderr, "invalid --port value\n");
                return false;
            }
        } else if (std::strcmp(arg, "--sample-ms") == 0) {
            const char* value = require_value("--sample-ms");
            if (value == nullptr || !parse_int(value, &out->sample_ms)) {
                std::fprintf(stderr, "invalid --sample-ms value\n");
                return false;
            }
        } else if (std::strcmp(arg, "--max-messages") == 0) {
            const char* value = require_value("--max-messages");
            if (value == nullptr || !parse_int(value, &out->max_messages)) {
                std::fprintf(stderr, "invalid --max-messages value\n");
                return false;
            }
        } else if (std::strcmp(arg, "--dump-frames") == 0) {
            out->dump_frames = true;
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg);
            return false;
        }
    }

    if (out->tracker == nullptr || out->tracker[0] == '\0') {
        std::fprintf(stderr, "--tracker is required\n");
        return false;
    }
    if (out->port <= 0 || out->port > 65535) {
        std::fprintf(stderr, "--port must be between 1 and 65535\n");
        return false;
    }
    if (out->sample_ms < 1) {
        out->sample_ms = 1;
    }
    if (out->max_messages < 0) {
        out->max_messages = 0;
    }
    return true;
}

Rpy quat_to_rpy(const raw_vrpn::Pose& pose) {
    Rpy out;
    const double qx = pose.qx;
    const double qy = pose.qy;
    const double qz = pose.qz;
    const double qw = pose.qw;

    const double sinr_cosp = 2.0 * (qw * qx + qy * qz);
    const double cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy);
    out.roll = std::atan2(sinr_cosp, cosr_cosp);

    const double sinp = 2.0 * (qw * qy - qz * qx);
    if (std::fabs(sinp) >= 1.0) {
        out.pitch = std::copysign(M_PI / 2.0, sinp);
    } else {
        out.pitch = std::asin(sinp);
    }

    const double siny_cosp = 2.0 * (qw * qz + qx * qy);
    const double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
    out.yaw = std::atan2(siny_cosp, cosy_cosp);
    return out;
}

bool handshake(int fd) {
    char error[256] = {};
    char cookie[24] = {};
    raw_vrpn::make_cookie(cookie, sizeof(cookie));

    if (!raw_vrpn::write_all(fd, reinterpret_cast<const std::uint8_t*>(cookie), sizeof(cookie), error, sizeof(error))) {
        std::fprintf(stderr, "failed to send VRPN cookie: %s\n", error);
        return false;
    }

    char server_cookie[24] = {};
    const raw_vrpn::ReadStatus status =
        raw_vrpn::read_exact(fd, reinterpret_cast<std::uint8_t*>(server_cookie), sizeof(server_cookie), 3000, error, sizeof(error));
    if (status != raw_vrpn::ReadStatus::Ok) {
        std::fprintf(stderr, "failed to read VRPN cookie: %s\n", error[0] ? error : "timeout or closed socket");
        return false;
    }
    if (!raw_vrpn::cookie_major_matches(server_cookie, sizeof(server_cookie))) {
        std::fprintf(stderr, "server did not return a compatible VRPN cookie\n");
        return false;
    }
    return true;
}

const char* read_status_text(raw_vrpn::ReadStatus status) {
    switch (status) {
        case raw_vrpn::ReadStatus::Ok:
            return "ok";
        case raw_vrpn::ReadStatus::Timeout:
            return "timeout";
        case raw_vrpn::ReadStatus::Closed:
            return "closed";
        case raw_vrpn::ReadStatus::Error:
            return "error";
    }
    return "unknown";
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    Options options;
    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }

    char error[256] = {};
    const int fd = raw_vrpn::tcp_connect(options.host, options.port, error, sizeof(error));
    if (fd < 0) {
        std::fprintf(stderr, "connect failed: %s\n", error);
        return 1;
    }

    if (!handshake(fd)) {
        raw_vrpn::close_socket(fd);
        return 1;
    }

    raw_vrpn::Registry registry{};
    std::uint8_t header_bytes[raw_vrpn::kHeaderSize] = {};
    std::uint8_t payload[8192] = {};
    int total_messages = 0;
    int window_messages = 0;
    auto window_start = std::chrono::steady_clock::now();

    while (!g_should_exit) {
        raw_vrpn::FrameHeader header{};
        raw_vrpn::ReadStatus status =
            raw_vrpn::read_exact(fd, header_bytes, sizeof(header_bytes), 1000, error, sizeof(error));
        if (status == raw_vrpn::ReadStatus::Timeout) {
            continue;
        }
        if (status != raw_vrpn::ReadStatus::Ok) {
            std::fprintf(stderr, "failed to read frame header: %s\n", error[0] ? error : read_status_text(status));
            raw_vrpn::close_socket(fd);
            return 1;
        }
        if (!raw_vrpn::decode_frame_header(header_bytes, sizeof(header_bytes), &header)) {
            std::fprintf(stderr, "invalid VRPN frame header\n");
            raw_vrpn::close_socket(fd);
            return 1;
        }

        const std::size_t padded_len = raw_vrpn::aligned_payload_len(header.payload_len);
        if (padded_len > sizeof(payload)) {
            status = raw_vrpn::discard_exact(fd, padded_len, 1000, error, sizeof(error));
            if (status != raw_vrpn::ReadStatus::Ok) {
                std::fprintf(stderr, "failed to discard oversized payload: %s\n", error[0] ? error : read_status_text(status));
                raw_vrpn::close_socket(fd);
                return 1;
            }
            continue;
        }

        status = raw_vrpn::read_exact(fd, payload, padded_len, 1000, error, sizeof(error));
        if (status != raw_vrpn::ReadStatus::Ok) {
            std::fprintf(stderr, "failed to read frame payload: %s\n", error[0] ? error : read_status_text(status));
            raw_vrpn::close_socket(fd);
            return 1;
        }

        if (options.dump_frames) {
            const char* sender_name = raw_vrpn::find_sender_name(&registry, header.sender);
            const char* type_name = raw_vrpn::find_type_name(&registry, header.type);
            std::fprintf(stderr,
                         "frame sender=%d(%s) type=%d(%s) payload=%u seq=%u\n",
                         header.sender,
                         sender_name ? sender_name : "?",
                         header.type,
                         type_name ? type_name : "?",
                         header.payload_len,
                         header.sequence);
        }

        if (header.type == raw_vrpn::kSenderDescriptionType || header.type == raw_vrpn::kTypeDescriptionType) {
            if (!raw_vrpn::handle_description_message(&registry, header.type, header.sender, payload, header.payload_len)) {
                std::fprintf(stderr, "warning: failed to parse VRPN description message\n");
            }
            continue;
        }

        if (!raw_vrpn::is_target_pose_message(&registry, header.sender, header.type, options.tracker)) {
            continue;
        }

        raw_vrpn::Pose pose{};
        if (!raw_vrpn::decode_pos_quat_payload(payload, header.payload_len, &pose)) {
            std::fprintf(stderr, "warning: bad Pos_Quat payload length=%u\n", header.payload_len);
            continue;
        }

        ++total_messages;
        ++window_messages;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - window_start).count();
        const double hz = elapsed > 0.0 ? static_cast<double>(window_messages) / elapsed : 0.0;
        const Rpy rpy = quat_to_rpy(pose);
        const double timestamp = static_cast<double>(header.time_sec) + static_cast<double>(header.time_usec) / 1000000.0;

        std::printf("ts=%.6f | pos=(%.4f, %.4f, %.4f) | quat=(%.6f, %.6f, %.6f, %.6f) | rpy=(%.4f, %.4f, %.4f) | count=%d | hz=%.2f\n",
                    timestamp,
                    pose.x,
                    pose.y,
                    pose.z,
                    pose.qx,
                    pose.qy,
                    pose.qz,
                    pose.qw,
                    rpy.roll,
                    rpy.pitch,
                    rpy.yaw,
                    total_messages,
                    hz);
        std::fflush(stdout);

        if (elapsed >= 1.0) {
            window_start = now;
            window_messages = 0;
        }
        if (options.max_messages > 0 && total_messages >= options.max_messages) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(options.sample_ms));
    }

    raw_vrpn::close_socket(fd);
    return 0;
}
