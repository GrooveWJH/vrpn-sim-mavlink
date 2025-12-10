#include "receiver/TrackerClient.h"

#include <vrpn_Tracker.h>

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace receiver {
namespace {

Pose from_tracker_cb(const vrpn_TRACKERCB& info) {
    Pose pose;
    pose.timestamp_sec = info.msg_time.tv_sec + info.msg_time.tv_usec / 1e6;
    pose.x             = info.pos[0];
    pose.y             = info.pos[1];
    pose.z             = info.pos[2];

    const double qx = info.quat[0];
    const double qy = info.quat[1];
    const double qz = info.quat[2];
    const double qw = info.quat[3];

    const double sinr_cosp = 2.0 * (qw * qx + qy * qz);
    const double cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy);
    pose.roll              = std::atan2(sinr_cosp, cosr_cosp);

    const double sinp = 2.0 * (qw * qy - qz * qx);
    if (std::abs(sinp) >= 1.0) {
        pose.pitch = std::copysign(M_PI / 2.0, sinp);
    } else {
        pose.pitch = std::asin(sinp);
    }

    const double siny_cosp = 2.0 * (qw * qz + qx * qy);
    const double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
    pose.yaw               = std::atan2(siny_cosp, cosy_cosp);
    return pose;
}

}  // namespace

TrackerClient::TrackerClient(const std::string& address)
    : address_(address) {
    create_tracker();
}

TrackerClient::~TrackerClient() {
    destroy_tracker();
}

bool TrackerClient::spin_once() {
    if (!tracker_) {
        create_tracker();
        return tracker_ != nullptr;
    }
    tracker_->mainloop();
    if (auto* conn = tracker_->connectionPtr()) {
        conn->mainloop();
        if (active_ && !conn->doing_okay()) {
            destroy_tracker();
            return false;
        }
    }
    return true;
}

std::optional<Pose> TrackerClient::latest_pose() const {
    if (!have_pose_) {
        return std::nullopt;
    }
    return last_pose_;
}

void VRPN_CALLBACK TrackerClient::handle_tracker(void* userdata, const vrpn_TRACKERCB info) {
    auto* self = static_cast<TrackerClient*>(userdata);
    self->last_pose_ = from_tracker_cb(info);
    self->have_pose_ = true;
    self->active_    = true;
}

void TrackerClient::create_tracker() {
    destroy_tracker();
    tracker_ = new vrpn_Tracker_Remote(address_.c_str());
    if (!tracker_) {
        throw std::runtime_error("Failed to create vrpn_Tracker_Remote");
    }
    tracker_->register_change_handler(this, &TrackerClient::handle_tracker);
    have_pose_ = false;
    active_    = false;
}

void TrackerClient::destroy_tracker() {
    if (tracker_) {
        tracker_->unregister_change_handler(this, &TrackerClient::handle_tracker);
        delete tracker_;
        tracker_ = nullptr;
    }
    have_pose_ = false;
    active_    = false;
}

}  // namespace receiver
