#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace sancak::net {

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
    std::uint64_t timestamp_us; // Kamera kare alındığı an (Unix epoch, system_clock) mikrosaniye
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

inline UdpJpegFragmentHeader makeUdpHeader(
    std::uint32_t frame_id,
    std::uint64_t timestamp_us,
    std::uint16_t chunk_index,
    std::uint16_t chunk_count,
    std::uint32_t jpeg_bytes,
    std::uint32_t chunk_offset,
    std::uint16_t chunk_bytes)
{
    UdpJpegFragmentHeader h{};
    std::memcpy(h.magic, kUdpMagic.data(), kUdpMagic.size());
    h.version = kProtocolVersion;
    h.flags = 0;
    h.header_bytes = static_cast<std::uint16_t>(sizeof(UdpJpegFragmentHeader));
    h.frame_id = frame_id;
    h.timestamp_us = timestamp_us;
    h.chunk_index = chunk_index;
    h.chunk_count = chunk_count;
    h.jpeg_bytes = jpeg_bytes;
    h.chunk_offset = chunk_offset;
    h.chunk_bytes = chunk_bytes;
    h.reserved = 0;
    return h;
}

inline bool isUdpFragment(const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(UdpJpegFragmentHeader)) return false;
    return std::memcmp(data, kUdpMagic.data(), kUdpMagic.size()) == 0;
}

inline TcpMsgHeader makeTcpHeader(TcpMsgType type, std::uint16_t payload_bytes) {
    TcpMsgHeader h{};
    std::memcpy(h.magic, kTcpMagic.data(), kTcpMagic.size());
    h.version = kProtocolVersion;
    h.type = static_cast<std::uint8_t>(type);
    h.payload_bytes = payload_bytes;
    return h;
}

inline bool isTcpFrame(const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(TcpMsgHeader)) return false;
    return std::memcmp(data, kTcpMagic.data(), kTcpMagic.size()) == 0;
}

// Little-endian writer helpers (explicit, avoids UB across platforms)
inline void appendBytes(std::vector<std::uint8_t>& out, const void* src, std::size_t n) {
    const auto* p = static_cast<const std::uint8_t*>(src);
    out.insert(out.end(), p, p + n);
}

inline void appendU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

inline void appendI32LE(std::vector<std::uint8_t>& out, std::int32_t v) {
    appendU32LE(out, static_cast<std::uint32_t>(v));
}

inline void appendF32LE(std::vector<std::uint8_t>& out, float v) {
    static_assert(sizeof(float) == 4, "float must be 32-bit IEEE754");
    std::uint32_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    appendU32LE(out, u);
}

} // namespace sancak::net
