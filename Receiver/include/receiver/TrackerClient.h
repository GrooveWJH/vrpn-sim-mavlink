#pragma once

#include <optional>
#include <string>

#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>

namespace receiver {

struct Pose {
    double timestamp_sec = 0.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
};

class TrackerClient {
public:
    explicit TrackerClient(const std::string& address);
    ~TrackerClient();

    bool spin_once();
    std::optional<Pose> latest_pose() const;

private:
    void create_tracker();
    void destroy_tracker();

    static void VRPN_CALLBACK handle_tracker(void* userdata, const vrpn_TRACKERCB info);

    std::string address_;
    vrpn_Tracker_Remote* tracker_ = nullptr;
    Pose last_pose_{};
    bool have_pose_ = false;
    bool active_ = false;
};

}  // namespace receiver
