#pragma once

#include <cstddef>
#include <cstdint>

namespace raw_vrpn {

enum class ReadStatus {
    Ok,
    Timeout,
    Closed,
    Error,
};

int tcp_connect(const char* host, int port, char* error, std::size_t error_len);
void close_socket(int fd);
bool write_all(int fd, const std::uint8_t* data, std::size_t len, char* error, std::size_t error_len);
ReadStatus read_exact(int fd,
                      std::uint8_t* data,
                      std::size_t len,
                      int timeout_ms,
                      char* error,
                      std::size_t error_len);
ReadStatus discard_exact(int fd, std::size_t len, int timeout_ms, char* error, std::size_t error_len);

}  // namespace raw_vrpn
