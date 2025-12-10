#include "vrpn_sim/ProgramOptions.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vrpn_sim {
namespace {

const char* program_name(const char* exe) {
    if (exe == nullptr) {
        return "fake_vrpn_uav_server";
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

void print_help(const char* exe) {
    const char* prog = program_name(exe);
    std::printf("Usage: %s [options]\n", prog);
    std::printf("  -b, --bind <addr>          VRPN bind string (default :3883)\n");
    std::printf("  -n, --num-trackers <N>     Number of trackers to simulate (default 32)\n");
    std::printf("  -r, --rate <Hz>            Publish rate (default 50Hz)\n");
    std::printf("  -q, --quiet                Suppress periodic status output\n");
    std::printf("      --status-interval <s>  Seconds between status logs (default 5)\n");
    std::printf(
        "      --status-mode <mode>   'append' (default) or 'inline' reuse one terminal line\n");
    std::printf("      --status-pose          Append pose/quaternion data to status logs (default)\n");
    std::printf("      --status-no-pose       Suppress pose/quaternion data in status logs\n");
    std::printf(
        "      --status-tracker <id>  Tracker index used for status pose output (default 0)\n");
    std::printf("      --auto-restart         Retry binding after errors (default: disabled)\n");
    std::printf("      --restart-delay <s>    Delay before auto-restart (default 1s)\n");
    std::printf("\nExamples:\n");
    std::printf("  %s --bind :3883 --num-trackers 32 --rate 50\n", prog);
    std::printf("  %s --bind :4000 --auto-restart --restart-delay 2.0\n", prog);
    std::printf("  %s --bind :3883 -q --status-interval 10 --status-mode inline\n", prog);
}

}  // namespace

ProgramOptions parse_args(int argc, char** argv) {
    ProgramOptions opts;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if ((std::strcmp(arg, "-b") == 0 || std::strcmp(arg, "--bind") == 0) && i + 1 < argc) {
            opts.bind_address = argv[++i];
        } else if ((std::strcmp(arg, "-n") == 0 || std::strcmp(arg, "--num-trackers") == 0) &&
                   i + 1 < argc) {
            opts.tracker_count = std::atoi(argv[++i]);
        } else if ((std::strcmp(arg, "-r") == 0 || std::strcmp(arg, "--rate") == 0) &&
                   i + 1 < argc) {
            opts.publish_rate_hz = std::atof(argv[++i]);
        } else if (std::strcmp(arg, "-q") == 0 || std::strcmp(arg, "--quiet") == 0) {
            opts.quiet = true;
        } else if (std::strcmp(arg, "--status-interval") == 0 && i + 1 < argc) {
            opts.status_interval_s = std::atof(argv[++i]);
        } else if (std::strcmp(arg, "--status-mode") == 0 && i + 1 < argc) {
            const char* mode = argv[++i];
            if (std::strcmp(mode, "append") == 0) {
                opts.status_single_line = false;
            } else if (std::strcmp(mode, "inline") == 0 || std::strcmp(mode, "single-line") == 0) {
                opts.status_single_line = true;
            } else {
                std::fprintf(stderr, "Unknown status mode '%s'. Use 'append' or 'inline'.\n", mode);
                std::exit(1);
            }
        } else if (std::strcmp(arg, "--status-pose") == 0) {
            opts.status_include_pose = true;
        } else if (std::strcmp(arg, "--status-no-pose") == 0) {
            opts.status_include_pose = false;
        } else if (std::strcmp(arg, "--status-tracker") == 0 && i + 1 < argc) {
            opts.status_pose_tracker = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--auto-restart") == 0) {
            opts.auto_restart = true;
        } else if (std::strcmp(arg, "--restart-delay") == 0 && i + 1 < argc) {
            opts.restart_delay_s = std::atof(argv[++i]);
        } else if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            print_help(argv[0]);
            std::exit(0);
        }
    }
    return opts;
}

}  // namespace vrpn_sim
