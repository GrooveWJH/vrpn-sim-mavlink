#include "receiver/MavlinkSender.h"

#include <common/mavlink.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

namespace receiver {
namespace {

speed_t to_speed_t(int baud_rate) {
    switch (baud_rate) {
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return B57600;
    }
}

std::pair<std::string, uint16_t> parse_udp_target(const std::string& target) {
    auto pos = target.find(':');
    if (pos == std::string::npos) {
        throw std::invalid_argument("UDP target must be host:port");
    }
    std::string host = target.substr(0, pos);
    uint16_t port    = static_cast<uint16_t>(std::stoi(target.substr(pos + 1)));
    return {host, port};
}

}  // namespace

MavlinkSender::MavlinkSender(const MavlinkOptions& options)
    : system_id_(options.system_id), component_id_(options.component_id) {
    if (options.link_type == "serial") {
        use_udp_ = false;
        open_serial(options.serial_device, options.baud_rate);
    } else if (options.link_type == "udp") {
        use_udp_ = true;
        open_udp(options.udp_target);
    } else {
        throw std::invalid_argument("Unknown link type: " + options.link_type);
    }
}

MavlinkSender::~MavlinkSender() {
    if (use_udp_) {
        if (udp_socket_ >= 0) {
            ::close(udp_socket_);
        }
    } else if (fd_ >= 0) {
        ::close(fd_);
    }
}

void MavlinkSender::send_pose(const Pose& pose) {
    mavlink_message_t message;
    const uint64_t usec = static_cast<uint64_t>(pose.timestamp_sec * 1e6);
    float covariance[21] = {0.0f};
    mavlink_msg_vision_position_estimate_pack(
        system_id_,
        component_id_,
        &message,
        usec,
        static_cast<float>(pose.x),
        static_cast<float>(pose.y),
        static_cast<float>(pose.z),
        static_cast<float>(pose.roll),
        static_cast<float>(pose.pitch),
        static_cast<float>(pose.yaw),
        covariance,
        0);

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
    write_bytes(buffer, length);
}

void MavlinkSender::open_serial(const std::string& device, int baud_rate) {
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open serial device: " + device + " error: " + std::strerror(errno));
    }
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        throw std::runtime_error("tcgetattr failed");
    }
    cfmakeraw(&tty);
    cfsetspeed(&tty, to_speed_t(baud_rate));
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        throw std::runtime_error("tcsetattr failed");
    }
}

void MavlinkSender::open_udp(const std::string& target) {
    auto [host, port] = parse_udp_target(target);
    udp_socket_       = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_ < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }
    std::memset(&udp_addr_, 0, sizeof(udp_addr_));
    udp_addr_.sin_family = AF_INET;
    udp_addr_.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &udp_addr_.sin_addr) != 1) {
        throw std::invalid_argument("Invalid UDP host: " + host);
    }
}

void MavlinkSender::write_bytes(const uint8_t* data, size_t length) {
    if (use_udp_) {
        if (::sendto(udp_socket_, data, length, 0, reinterpret_cast<sockaddr*>(&udp_addr_), sizeof(udp_addr_)) < 0) {
            throw std::runtime_error("sendto failed");
        }
    } else {
        if (::write(fd_, data, length) < 0) {
            throw std::runtime_error("write failed");
        }
    }
}

}  // namespace receiver
