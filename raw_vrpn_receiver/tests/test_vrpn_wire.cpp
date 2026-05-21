#include "raw_vrpn_receiver/vrpn_wire.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void append_i32(std::vector<std::uint8_t>& out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}

void append_f64(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((bits >> (i * 8)) & 0xff));
    }
}

std::vector<std::uint8_t> make_description_payload(const char* name) {
    std::vector<std::uint8_t> payload;
    const std::uint32_t len = static_cast<std::uint32_t>(std::strlen(name) + 1);
    append_u32(payload, len);
    payload.insert(payload.end(), name, name + len);
    return payload;
}

std::vector<std::uint8_t> make_pos_quat_payload() {
    std::vector<std::uint8_t> payload;
    append_i32(payload, 0);
    append_i32(payload, 0);
    append_f64(payload, 1.25);
    append_f64(payload, -2.5);
    append_f64(payload, 3.75);
    append_f64(payload, 0.1);
    append_f64(payload, 0.2);
    append_f64(payload, 0.3);
    append_f64(payload, 0.9);
    return payload;
}

void test_endian_helpers() {
    const std::uint8_t i32_bytes[] = {0xff, 0xff, 0xff, 0xfe};
    const std::uint8_t u32_bytes[] = {0x12, 0x34, 0x56, 0x78};
    std::vector<std::uint8_t> f64_bytes;
    append_f64(f64_bytes, 42.25);

    assert(raw_vrpn::read_be_i32(i32_bytes) == -2);
    assert(raw_vrpn::read_be_u32(u32_bytes) == 0x12345678u);
    assert(std::fabs(raw_vrpn::read_be_f64(f64_bytes.data()) - 42.25) < 1e-12);
}

void test_frame_header_and_padding() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, raw_vrpn::kHeaderSize + 5);
    append_u32(bytes, 10);
    append_u32(bytes, 20);
    append_i32(bytes, 7);
    append_i32(bytes, -2);
    append_u32(bytes, 99);

    raw_vrpn::FrameHeader header{};
    assert(raw_vrpn::decode_frame_header(bytes.data(), bytes.size(), &header));
    assert(header.payload_len == 5);
    assert(header.time_sec == 10);
    assert(header.time_usec == 20);
    assert(header.sender == 7);
    assert(header.type == -2);
    assert(header.sequence == 99);
    assert(raw_vrpn::aligned_payload_len(header.payload_len) == 8);
}

void test_description_tables() {
    raw_vrpn::Registry registry{};
    auto sender_payload = make_description_payload("sunraynext_sim0");
    auto type_payload = make_description_payload("vrpn_Tracker Pos_Quat");

    assert(raw_vrpn::handle_description_message(
        &registry, raw_vrpn::kSenderDescriptionType, 4, sender_payload.data(), sender_payload.size()));
    assert(raw_vrpn::handle_description_message(
        &registry, raw_vrpn::kTypeDescriptionType, 9, type_payload.data(), type_payload.size()));

    assert(std::strcmp(raw_vrpn::find_sender_name(&registry, 4), "sunraynext_sim0") == 0);
    assert(std::strcmp(raw_vrpn::find_type_name(&registry, 9), "vrpn_Tracker Pos_Quat") == 0);
}

void test_pos_quat_decode() {
    auto payload = make_pos_quat_payload();
    raw_vrpn::Pose pose{};
    assert(raw_vrpn::decode_pos_quat_payload(payload.data(), payload.size(), &pose));
    assert(pose.sensor == 0);
    assert(std::fabs(pose.x - 1.25) < 1e-12);
    assert(std::fabs(pose.y + 2.5) < 1e-12);
    assert(std::fabs(pose.z - 3.75) < 1e-12);
    assert(std::fabs(pose.qx - 0.1) < 1e-12);
    assert(std::fabs(pose.qy - 0.2) < 1e-12);
    assert(std::fabs(pose.qz - 0.3) < 1e-12);
    assert(std::fabs(pose.qw - 0.9) < 1e-12);
}

void test_rejects_bad_payloads() {
    raw_vrpn::Registry registry{};
    raw_vrpn::Pose pose{};
    const std::uint8_t too_short[] = {0, 1, 2};

    assert(!raw_vrpn::decode_pos_quat_payload(too_short, sizeof(too_short), &pose));
    assert(raw_vrpn::find_sender_name(&registry, 404) == nullptr);
    assert(raw_vrpn::find_type_name(&registry, 404) == nullptr);
}

void test_cookie_handling() {
    char cookie[25] = {};
    cookie[24] = static_cast<char>(0x7f);

    raw_vrpn::make_cookie(cookie, 24);

    assert(cookie[24] == static_cast<char>(0x7f));
    assert(raw_vrpn::cookie_major_matches(cookie, 24));
    assert(raw_vrpn::cookie_major_matches("vrpn: ver. 07.34  0", 24));
    assert(!raw_vrpn::cookie_major_matches("vrpn: ver. 08.00  0", 24));

    char tiny[5] = {};
    tiny[4] = static_cast<char>(0x7f);
    raw_vrpn::make_cookie(tiny, 4);
    assert(tiny[4] == static_cast<char>(0x7f));
}

}  // namespace

int main() {
    test_endian_helpers();
    test_frame_header_and_padding();
    test_description_tables();
    test_pos_quat_decode();
    test_rejects_bad_payloads();
    test_cookie_handling();
    return 0;
}
