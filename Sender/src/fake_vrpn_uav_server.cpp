#include "vrpn_sim/FakeTrackerServer.h"
#include "vrpn_sim/ProgramOptions.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        auto options = vrpn_sim::parse_args(argc, argv);
        vrpn_sim::install_signal_handlers();
        vrpn_sim::FakeTrackerServer server(std::move(options));
        return server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }
}
