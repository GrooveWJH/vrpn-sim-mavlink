#pragma once

#include <string>

namespace vrpn_sim {

struct ProgramOptions {
    std::string bind_address = ":3883";
    int tracker_count        = 32;
    double publish_rate_hz   = 50.0;
    bool quiet               = false;
    double status_interval_s = 5.0;
    bool auto_restart        = false;
    double restart_delay_s   = 1.0;
    bool status_single_line  = false;
    bool status_include_pose = true;
    int status_pose_tracker  = 0;
};

ProgramOptions parse_args(int argc, char** argv);

}  // namespace vrpn_sim
