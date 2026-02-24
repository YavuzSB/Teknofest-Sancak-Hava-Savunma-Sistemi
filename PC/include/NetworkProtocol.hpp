#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace teknofest::net {

inline constexpr std::array<std::uint8_t, 4> kUdpMagic{'S', 'N', 'K', '1'};
inline constexpr std::array<std::uint8_t, 4> kTcpMagic{'S', 'N', 'K', '2'};
inline constexpr std::uint8_t kProtocolVersion = 1;

enum class TcpMsgType : std::uint8_t {
    kAimResultV1 = 1,
};

#pragma pack(push, 1)
struct UdpJpegFragmentHeader {
    std::uint8_t magic[4];
    std::uint8_t version;
    std::uint8_t flags;
    std::uint16_t header_bytes;
    std::uint32_t frame_id;
    std::uint64_t timestamp_us; // Kamera kare alındığı an (Unix epoch, system_clock) mikrosaniye (raspi'den gelir)
    std::uint16_t chunk_index;
    std::uint16_t chunk_count;
    std::uint32_t jpeg_bytes;
    std::uint32_t chunk_offset;
    std::uint16_t chunk_bytes;
    std::uint16_t reserved;
};

struct TcpMsgHeader {
    std::uint8_t magic[4];
    std::uint8_t version;
    std::uint8_t type;
    std::uint16_t payload_bytes;
};
#pragma pack(pop)

static_assert(sizeof(UdpJpegFragmentHeader) == 36, "Unexpected UdpJpegFragmentHeader size");
static_assert(sizeof(TcpMsgHeader) == 8, "Unexpected TcpMsgHeader size");

inline bool isUdpFragment(const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(UdpJpegFragmentHeader)) return false;
    return std::memcmp(data, kUdpMagic.data(), kUdpMagic.size()) == 0;
}

inline bool isTcpFrame(const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(TcpMsgHeader)) return false;
    return std::memcmp(data, kTcpMagic.data(), kTcpMagic.size()) == 0;
}

} // namespace teknofest::net
