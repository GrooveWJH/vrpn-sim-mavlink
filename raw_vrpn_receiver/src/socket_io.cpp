#include "raw_vrpn_receiver/socket_io.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace raw_vrpn {
namespace {

void set_error(char* error, std::size_t error_len, const char* message) {
    if (error == nullptr || error_len == 0) {
        return;
    }
    std::snprintf(error, error_len, "%s", message == nullptr ? "unknown error" : message);
}

void set_errno_error(char* error, std::size_t error_len, const char* context) {
    if (error == nullptr || error_len == 0) {
        return;
    }
    std::snprintf(error, error_len, "%s: %s", context, std::strerror(errno));
}

}  // namespace

int tcp_connect(const char* host, int port, char* error, std::size_t error_len) {
    if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
        set_error(error, error_len, "invalid host or port");
        return -1;
    }

    char port_text[16] = {};
    std::snprintf(port_text, sizeof(port_text), "%d", port);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    const int gai = getaddrinfo(host, port_text, &hints, &results);
    if (gai != 0) {
        set_error(error, error_len, gai_strerror(gai));
        return -1;
    }

    int fd = -1;
    for (addrinfo* it = results; it != nullptr; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(results);
    if (fd < 0) {
        set_errno_error(error, error_len, "connect");
    }
    return fd;
}

void close_socket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

bool write_all(int fd, const std::uint8_t* data, std::size_t len, char* error, std::size_t error_len) {
    std::size_t offset = 0;
    while (offset < len) {
        const ssize_t wrote = send(fd, data + offset, len - offset, 0);
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_errno_error(error, error_len, "send");
            return false;
        }
        if (wrote == 0) {
            set_error(error, error_len, "socket closed during send");
            return false;
        }
        offset += static_cast<std::size_t>(wrote);
    }
    return true;
}

ReadStatus read_exact(int fd,
                      std::uint8_t* data,
                      std::size_t len,
                      int timeout_ms,
                      char* error,
                      std::size_t error_len) {
    std::size_t offset = 0;
    while (offset < len) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        timeval timeout{};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                return ReadStatus::Timeout;
            }
            set_errno_error(error, error_len, "select");
            return ReadStatus::Error;
        }
        if (ready == 0) {
            return ReadStatus::Timeout;
        }

        const ssize_t got = recv(fd, data + offset, len - offset, 0);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_errno_error(error, error_len, "recv");
            return ReadStatus::Error;
        }
        if (got == 0) {
            return ReadStatus::Closed;
        }
        offset += static_cast<std::size_t>(got);
    }
    return ReadStatus::Ok;
}

ReadStatus discard_exact(int fd, std::size_t len, int timeout_ms, char* error, std::size_t error_len) {
    std::uint8_t scratch[256];
    std::size_t remaining = len;
    while (remaining > 0) {
        const std::size_t chunk = remaining < sizeof(scratch) ? remaining : sizeof(scratch);
        const ReadStatus status = read_exact(fd, scratch, chunk, timeout_ms, error, error_len);
        if (status != ReadStatus::Ok) {
            return status;
        }
        remaining -= chunk;
    }
    return ReadStatus::Ok;
}

}  // namespace raw_vrpn
