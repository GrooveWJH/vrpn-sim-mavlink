#include "raw_vrpn_receiver/vrpn_wire.h"

#include <cstdio>
#include <cstring>

namespace raw_vrpn {
namespace {

constexpr const char* kCookiePrefix = "vrpn: ver. 07.";
constexpr const char* kClientCookie = "vrpn: ver. 07.36  0";
constexpr std::size_t kCookieSize = 24;

NameEntry* find_slot(NameEntry* entries, std::int32_t id) {
    NameEntry* first_free = nullptr;
    for (std::size_t i = 0; i < kMaxRegistryEntries; ++i) {
        if (entries[i].used && entries[i].id == id) {
            return &entries[i];
        }
        if (!entries[i].used && first_free == nullptr) {
            first_free = &entries[i];
        }
    }
    return first_free;
}

const char* find_name(const NameEntry* entries, std::int32_t id) {
    for (std::size_t i = 0; i < kMaxRegistryEntries; ++i) {
        if (entries[i].used && entries[i].id == id) {
            return entries[i].name;
        }
    }
    return nullptr;
}

bool upsert_name(NameEntry* entries, std::int32_t id, const char* name, std::size_t len) {
    if (len == 0 || len > kNameMax) {
        return false;
    }

    NameEntry* slot = find_slot(entries, id);
    if (slot == nullptr) {
        return false;
    }

    slot->used = true;
    slot->id = id;
    std::memset(slot->name, 0, sizeof(slot->name));
    const std::size_t copy_len = len < kNameMax ? len : kNameMax - 1;
    std::memcpy(slot->name, name, copy_len);
    slot->name[kNameMax - 1] = '\0';
    return true;
}

}  // namespace

std::uint32_t read_be_u32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::int32_t read_be_i32(const std::uint8_t* data) {
    return static_cast<std::int32_t>(read_be_u32(data));
}

double read_be_f64(const std::uint8_t* data) {
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits = (bits << 8) | static_cast<std::uint64_t>(data[i]);
    }

    double value = 0.0;
    static_assert(sizeof(value) == sizeof(bits), "double must be 64-bit");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::size_t aligned_payload_len(std::size_t payload_len) {
    const std::size_t remainder = payload_len % kAlign;
    return remainder == 0 ? payload_len : payload_len + (kAlign - remainder);
}

bool decode_frame_header(const std::uint8_t* data, std::size_t len, FrameHeader* out) {
    if (data == nullptr || out == nullptr || len < kHeaderSize) {
        return false;
    }

    const std::uint32_t frame_len = read_be_u32(data);
    if (frame_len < kHeaderSize) {
        return false;
    }

    out->payload_len = frame_len - static_cast<std::uint32_t>(kHeaderSize);
    out->time_sec = read_be_u32(data + 4);
    out->time_usec = read_be_u32(data + 8);
    out->sender = read_be_i32(data + 12);
    out->type = read_be_i32(data + 16);
    out->sequence = read_be_u32(data + 20);
    return true;
}

bool handle_description_message(Registry* registry,
                                std::int32_t type,
                                std::int32_t sender,
                                const std::uint8_t* payload,
                                std::size_t payload_len) {
    if (registry == nullptr || payload == nullptr || payload_len < sizeof(std::uint32_t)) {
        return false;
    }
    if (type != kSenderDescriptionType && type != kTypeDescriptionType) {
        return false;
    }

    const std::uint32_t name_len = read_be_u32(payload);
    if (name_len == 0 || name_len > kNameMax || payload_len < sizeof(std::uint32_t) + name_len) {
        return false;
    }

    const char* name = reinterpret_cast<const char*>(payload + sizeof(std::uint32_t));
    if (name[name_len - 1] != '\0') {
        return false;
    }

    if (type == kSenderDescriptionType) {
        return upsert_name(registry->senders, sender, name, name_len);
    }
    return upsert_name(registry->types, sender, name, name_len);
}

const char* find_sender_name(const Registry* registry, std::int32_t id) {
    return registry == nullptr ? nullptr : find_name(registry->senders, id);
}

const char* find_type_name(const Registry* registry, std::int32_t id) {
    return registry == nullptr ? nullptr : find_name(registry->types, id);
}

bool decode_pos_quat_payload(const std::uint8_t* payload, std::size_t payload_len, Pose* out) {
    if (payload == nullptr || out == nullptr || payload_len != kPosQuatPayloadSize) {
        return false;
    }

    out->sensor = read_be_i32(payload);
    out->x = read_be_f64(payload + 8);
    out->y = read_be_f64(payload + 16);
    out->z = read_be_f64(payload + 24);
    out->qx = read_be_f64(payload + 32);
    out->qy = read_be_f64(payload + 40);
    out->qz = read_be_f64(payload + 48);
    out->qw = read_be_f64(payload + 56);
    return true;
}

bool is_target_pose_message(const Registry* registry,
                            std::int32_t sender,
                            std::int32_t type,
                            const char* tracker_name) {
    const char* sender_name = find_sender_name(registry, sender);
    const char* type_name = find_type_name(registry, type);
    return sender_name != nullptr && type_name != nullptr && tracker_name != nullptr &&
           std::strcmp(sender_name, tracker_name) == 0 &&
           std::strcmp(type_name, kTrackerPosQuatTypeName) == 0;
}

bool cookie_major_matches(const char* cookie, std::size_t len) {
    if (cookie == nullptr || len < std::strlen(kCookiePrefix)) {
        return false;
    }
    return std::strncmp(cookie, kCookiePrefix, std::strlen(kCookiePrefix)) == 0;
}

void make_cookie(char* out, std::size_t len) {
    if (out == nullptr || len == 0) {
        return;
    }
    char cookie[kCookieSize + 1] = {};
    std::snprintf(cookie, sizeof(cookie), "%.16s  %c", kClientCookie, '0');
    const std::size_t copy_len = len < kCookieSize ? len : kCookieSize;
    std::memcpy(out, cookie, copy_len);
}

}  // namespace raw_vrpn
