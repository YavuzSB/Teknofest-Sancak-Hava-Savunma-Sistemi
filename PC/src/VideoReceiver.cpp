/**
 * @file VideoReceiver.cpp
 * @brief UDP JPEG kare alıcısı – Winsock2 / POSIX soket + stb_image decode
 */
#include "VideoReceiver.hpp"

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

    while (m_running.load(std::memory_order_acquire)) {
        const int received = recvfrom(
            sock,
            reinterpret_cast<char*>(buffer.data()),
            kMaxDatagram, 0, nullptr, nullptr);

        if (received <= 0) continue;   // zaman aşımı veya hata – tekrar dene

        m_state.store(State::Receiving, std::memory_order_release);

        // JPEG → RGB piksel (stb_image)
        int w = 0, h = 0, ch = 0;
        uint8_t* pixels = stbi_load_from_memory(
            buffer.data(), received, &w, &h, &ch, 3);

        if (pixels && w > 0 && h > 0) {
            const size_t byteCount = static_cast<size_t>(w) * h * 3;
            std::lock_guard<std::mutex> lock(m_frameMutex);
            m_framePixels.assign(pixels, pixels + byteCount);
            m_frameWidth  = w;
            m_frameHeight = h;
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
