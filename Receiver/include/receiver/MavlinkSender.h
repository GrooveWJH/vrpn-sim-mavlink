#pragma once

#include "receiver/TrackerClient.h"

#include <cstdint>
#include <netinet/in.h>
#include <string>

namespace receiver {

struct MavlinkOptions {
    std::string link_type = "serial";  // "serial" or "udp"
    std::string serial_device = "/dev/ttyUSB0";
    int baud_rate = 921600;
    std::string udp_target = "127.0.0.1:14550";
    uint8_t system_id = 1;
    uint8_t component_id = 1;
};

class MavlinkSender {
public:
    explicit MavlinkSender(const MavlinkOptions& options);
    ~MavlinkSender();

    void send_pose(const Pose& pose);

private:
    void open_serial(const std::string& device, int baud_rate);
    void open_udp(const std::string& target);

    void write_bytes(const uint8_t* data, size_t length);

    int fd_ = -1;
    bool use_udp_ = false;
    uint8_t system_id_ = 1;
    uint8_t component_id_ = 1;

    // UDP specifics
    int udp_socket_ = -1;
    struct sockaddr_in udp_addr_{};
};

}  // namespace receiver
