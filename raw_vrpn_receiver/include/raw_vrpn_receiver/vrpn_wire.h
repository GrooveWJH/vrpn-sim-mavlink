#pragma once

#include <cstddef>
#include <cstdint>

namespace raw_vrpn {

constexpr std::size_t kAlign = 8;
constexpr std::size_t kHeaderSize = 24;
constexpr std::size_t kNameMax = 100;
constexpr std::size_t kMaxRegistryEntries = 128;
constexpr std::size_t kPosQuatPayloadSize = 64;
constexpr std::int32_t kSenderDescriptionType = -1;
constexpr std::int32_t kTypeDescriptionType = -2;
constexpr const char* kTrackerPosQuatTypeName = "vrpn_Tracker Pos_Quat";

struct FrameHeader {
    std::uint32_t payload_len = 0;
    std::uint32_t time_sec = 0;
    std::uint32_t time_usec = 0;
    std::int32_t sender = 0;
    std::int32_t type = 0;
    std::uint32_t sequence = 0;
};

struct NameEntry {
    bool used = false;
    std::int32_t id = 0;
    char name[kNameMax] = {};
};

struct Registry {
    NameEntry senders[kMaxRegistryEntries] = {};
    NameEntry types[kMaxRegistryEntries] = {};
};

struct Pose {
    std::int32_t sensor = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    double qw = 1.0;
};

std::uint32_t read_be_u32(const std::uint8_t* data);
std::int32_t read_be_i32(const std::uint8_t* data);
double read_be_f64(const std::uint8_t* data);

std::size_t aligned_payload_len(std::size_t payload_len);
bool decode_frame_header(const std::uint8_t* data, std::size_t len, FrameHeader* out);

bool handle_description_message(Registry* registry,
                                std::int32_t type,
                                std::int32_t sender,
                                const std::uint8_t* payload,
                                std::size_t payload_len);
const char* find_sender_name(const Registry* registry, std::int32_t id);
const char* find_type_name(const Registry* registry, std::int32_t id);

bool decode_pos_quat_payload(const std::uint8_t* payload, std::size_t payload_len, Pose* out);
bool is_target_pose_message(const Registry* registry,
                            std::int32_t sender,
                            std::int32_t type,
                            const char* tracker_name);

bool cookie_major_matches(const char* cookie, std::size_t len);
void make_cookie(char* out, std::size_t len);

}  // namespace raw_vrpn
