/**
 * @file VideoReceiver.cpp
 * @brief UDP JPEG kare alıcısı – Winsock2 / POSIX soket + stb_image decode
 */
#include "VideoReceiver.hpp"

#include <chrono>

// ── Platform soket soyutlaması ──────────────────────────────────────────────
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
    using socket_t = SOCKET;
    static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
    inline int closeSocket(socket_t s) { return closesocket(s); }
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
    using socket_t = int;
    static constexpr socket_t kInvalidSocket = -1;
    inline int closeSocket(socket_t s) { return close(s); }
#endif

// stb_image implementasyonu (tüm projede yalnızca burada)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "NetworkProtocol.hpp"

#include <chrono>
#include <unordered_map>

namespace teknofest {

// ── Ctor / Dtor ─────────────────────────────────────────────────────────────

VideoReceiver::VideoReceiver(uint16_t port, const std::string& bindIp)
    : m_port(port)
    , m_bindIp(bindIp)
{}

VideoReceiver::~VideoReceiver() {
    stop();
}

// ── Public API ──────────────────────────────────────────────────────────────

void VideoReceiver::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&VideoReceiver::receiverLoop, this);
}

void VideoReceiver::stop() {
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_state.store(State::Stopped, std::memory_order_release);
}

bool VideoReceiver::getLatestFrame(std::vector<uint8_t>& pixels,
                                    int& width, int& height) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_hasNewFrame) return false;

    pixels = std::move(m_framePixels);
    width  = m_frameWidth;
    height = m_frameHeight;

    if (m_frameTimestampUs != 0) {
        const auto now = std::chrono::system_clock::now();
        const auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        const double latencyMs = (static_cast<double>(nowUs) - static_cast<double>(m_frameTimestampUs)) / 1000.0;
        m_lastLatencyMs.store(latencyMs, std::memory_order_release);
    } else {
        m_lastLatencyMs.store(0.0, std::memory_order_release);
    }

    m_frameTimestampUs = 0;
    m_hasNewFrame = false;
    return true;
}

// ── Alıcı thread döngüsü ──────────────────────────────────────────────────

void VideoReceiver::receiverLoop() {
    m_state.store(State::Listening, std::memory_order_release);

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
        m_state.store(State::Error, std::memory_order_release);
        return;
    }

    // Okunma zaman aşımı (500 ms)
#ifdef _WIN32
    DWORD timeout = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv { 0, 500'000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // Alım arabelleğini büyüt (1 MB)
    int recvBufSize = 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&recvBufSize), sizeof(recvBufSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    inet_pton(AF_INET, m_bindIp.c_str(), &addr.sin_addr);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeSocket(sock);
        m_state.store(State::Error, std::memory_order_release);
        return;
    }

    constexpr int kMaxDatagram = 65535;
    std::vector<uint8_t> buffer(kMaxDatagram);

    struct Reassembly {
        std::vector<std::uint8_t> jpeg;
        std::vector<std::uint8_t> got;
        std::uint32_t jpegBytes = 0;
        std::uint16_t chunkCount = 0;
        std::uint16_t gotCount = 0;
        std::uint64_t timestampUs = 0;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    std::unordered_map<std::uint32_t, Reassembly> frames;

    auto cleanupOld = [&]() {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = frames.begin(); it != frames.end();) {
            const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastUpdate).count();
            if (ageMs > 500) {
                it = frames.erase(it);
            } else {
                ++it;
            }
        }
        // safety cap
        if (frames.size() > 32) {
            frames.clear();
        }
    };

    while (m_running.load(std::memory_order_acquire)) {
        const int received = recvfrom(
            sock,
            reinterpret_cast<char*>(buffer.data()),
            kMaxDatagram, 0, nullptr, nullptr);

        if (received <= 0) continue;   // zaman aşımı veya hata – tekrar dene

        m_state.store(State::Receiving, std::memory_order_release);

        // Yeni protokol (fragment) veya eski (tek datagram JPEG)
        const auto* data = buffer.data();
        const auto size = static_cast<std::size_t>(received);

        std::vector<std::uint8_t> jpegComplete;
        std::uint64_t frameTimestampUs = 0;

        if (teknofest::net::isUdpFragment(data, size)) {
            teknofest::net::UdpJpegFragmentHeader hdr{};
            std::memcpy(&hdr, data, sizeof(hdr));

            const bool headerOk =
                (hdr.version == teknofest::net::kProtocolVersion) &&
                (hdr.header_bytes == sizeof(teknofest::net::UdpJpegFragmentHeader)) &&
                (hdr.chunk_count > 0) &&
                (hdr.chunk_index < hdr.chunk_count) &&
                (hdr.jpeg_bytes > 0) &&
                (hdr.jpeg_bytes <= 4u * 1024u * 1024u) &&
                (hdr.chunk_bytes > 0) &&
                (hdr.chunk_offset + hdr.chunk_bytes <= hdr.jpeg_bytes) &&
                (size >= sizeof(hdr) + hdr.chunk_bytes);

            if (headerOk) {
                auto& st = frames[hdr.frame_id];
                const bool needReset =
                    (st.jpegBytes != hdr.jpeg_bytes) ||
                    (st.chunkCount != hdr.chunk_count) ||
                    st.jpeg.empty();

                if (needReset) {
                    st.jpegBytes = hdr.jpeg_bytes;
                    st.chunkCount = hdr.chunk_count;
                    st.gotCount = 0;
                    st.timestampUs = hdr.timestamp_us;
                    st.jpeg.assign(st.jpegBytes, 0);
                    st.got.assign(st.chunkCount, 0);
                }

                st.lastUpdate = std::chrono::steady_clock::now();

                const auto idx = static_cast<std::size_t>(hdr.chunk_index);
                if (idx < st.got.size() && st.got[idx] == 0) {
                    st.got[idx] = 1;
                    st.gotCount++;
                }

                std::memcpy(st.jpeg.data() + hdr.chunk_offset,
                            data + sizeof(hdr),
                            hdr.chunk_bytes);

                if (st.gotCount == st.chunkCount) {
                    jpegComplete = std::move(st.jpeg);
                    frameTimestampUs = st.timestampUs;
                    frames.erase(hdr.frame_id);
                }
            }

            cleanupOld();
        } else {
            // Eski davranış: tek UDP datagram = tek JPEG
            jpegComplete.assign(data, data + size);
        }

        if (jpegComplete.empty()) {
            continue;
        }

        // JPEG → RGB piksel (stb_image)
        int w = 0, h = 0, ch = 0;
        uint8_t* pixels = stbi_load_from_memory(
            jpegComplete.data(), static_cast<int>(jpegComplete.size()), &w, &h, &ch, 3);

        if (pixels && w > 0 && h > 0) {
            const size_t byteCount = static_cast<size_t>(w) * h * 3;
            std::lock_guard<std::mutex> lock(m_frameMutex);
            m_framePixels.assign(pixels, pixels + byteCount);
            m_frameWidth  = w;
            m_frameHeight = h;
            m_frameTimestampUs = frameTimestampUs;
            m_hasNewFrame = true;
        }

        if (pixels) {
            stbi_image_free(pixels);
        }
    }

    closeSocket(sock);
    m_state.store(State::Stopped, std::memory_order_release);
}

} // namespace teknofest
