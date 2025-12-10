#pragma once

#include <memory>

namespace vrpn_sim {

struct ProgramOptions;

void install_signal_handlers();

class FakeTrackerServer {
public:
    explicit FakeTrackerServer(ProgramOptions options);
    ~FakeTrackerServer();
    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace vrpn_sim
