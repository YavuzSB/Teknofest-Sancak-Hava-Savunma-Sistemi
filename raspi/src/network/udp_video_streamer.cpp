#include "network/udp_video_streamer.hpp"

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <stdexcept>

// ── Platform socket ─────────────────────────────────────────────────────────
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
    using socket_t = SOCKET;
    static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
    static int closeSocket(socket_t s) { return closesocket(s); }
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
    using socket_t = int;
    static constexpr socket_t kInvalidSocket = -1;
    static int closeSocket(socket_t s) { return ::close(s); }
#endif

namespace sancak::net {

struct UdpVideoStreamer::SocketHandle {
    socket_t sock = kInvalidSocket;
    sockaddr_in dest{};
};

UdpVideoStreamer::UdpVideoStreamer() = default;

UdpVideoStreamer::~UdpVideoStreamer() {
    stop();
}

bool UdpVideoStreamer::start(UdpVideoConfig cfg) {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }

    if (cfg.mtu_bytes < sizeof(UdpJpegFragmentHeader) + 64) {
        return false;
    }
    if (cfg.jpeg_quality < 10) cfg.jpeg_quality = 10;
    if (cfg.jpeg_quality > 95) cfg.jpeg_quality = 95;

    cfg_ = std::move(cfg);
    running_.store(true, std::memory_order_release);

    if (!openSocket()) {
        running_.store(false, std::memory_order_release);
        return false;
    }

    worker_ = std::thread(&UdpVideoStreamer::workerLoop, this);
    return true;
}

void UdpVideoStreamer::stop() {
    running_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(cv_mutex_);
        wake_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    closeSocket();

    std::lock_guard<std::mutex> lock(frame_mutex_);
    latest_frame_.release();
    has_frame_ = false;
}

void UdpVideoStreamer::submit(const cv::Mat& bgrFrame) {
    if (!running_.load(std::memory_order_acquire)) return;
    if (bgrFrame.empty()) return;

    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        latest_frame_ = bgrFrame.clone();
        const auto now = std::chrono::system_clock::now();
        latest_timestamp_us_ = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
        has_frame_ = true;
    }
    {
        std::lock_guard<std::mutex> lock(cv_mutex_);
        wake_ = true;
    }
    cv_.notify_one();
}

bool UdpVideoStreamer::openSocket() {
    if (sock_ != nullptr) return true;

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif

    auto* h = new SocketHandle();
    h->sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (h->sock == kInvalidSocket) {
        delete h;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // send buffer (4 MB)
    int sndBuf = 4 * 1024 * 1024;
    setsockopt(h->sock, SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<const char*>(&sndBuf), sizeof(sndBuf));

    h->dest.sin_family = AF_INET;
    h->dest.sin_port = htons(cfg_.port);

    if (inet_pton(AF_INET, cfg_.host.c_str(), &h->dest.sin_addr) != 1) {
        closeSocket(h->sock);
        delete h;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    sock_ = h;
    return true;
}

void UdpVideoStreamer::closeSocket() {
    if (!sock_) return;
    closeSocket(sock_->sock);
    delete sock_;
    sock_ = nullptr;
#ifdef _WIN32
    WSACleanup();
#endif
}

void UdpVideoStreamer::workerLoop() {
    // En fazla 60 FPS civarı gönderim hedefi; daha hızlı submit gelirse eski frame drop edilir.
    auto nextSend = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_acquire)) {
        // Wake veya 16ms timeout
        {
            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_until(lock, nextSend, [&] { return !running_.load() || wake_; });
            wake_ = false;
        }

        if (!running_.load(std::memory_order_acquire)) break;

        cv::Mat frame;
        std::uint64_t frameTimestampUs = 0;
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            if (!has_frame_) {
                nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
                continue;
            }
            frame = std::move(latest_frame_);
            frameTimestampUs = latest_timestamp_us_;
            latest_frame_.release();
            latest_timestamp_us_ = 0;
            has_frame_ = false;
        }

        if (frame.empty()) {
            nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
            continue;
        }

        std::vector<std::uint8_t> jpeg;
        try {
            std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, cfg_.jpeg_quality};
            cv::imencode(".jpg", frame, jpeg, params);
        } catch (...) {
            nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
            continue;
        }

        if (!jpeg.empty() && sock_) {
            sendJpeg(jpeg, frameTimestampUs);
        }

        nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
    }
}

void UdpVideoStreamer::sendJpeg(const std::vector<std::uint8_t>& jpeg, std::uint64_t frame_timestamp_us) {
    if (!sock_) return;

    const std::size_t headerBytes = sizeof(UdpJpegFragmentHeader);
    const std::size_t mtu = cfg_.mtu_bytes;
    const std::size_t maxPayload = (mtu > headerBytes) ? (mtu - headerBytes) : 0;
    if (maxPayload < 64) return;

    const std::uint32_t jpegBytes = static_cast<std::uint32_t>(jpeg.size());
    const std::uint32_t frameId = frame_id_.fetch_add(1, std::memory_order_acq_rel);

    const std::uint16_t chunkCount = static_cast<std::uint16_t>(
        (jpeg.size() + maxPayload - 1) / maxPayload);
    if (chunkCount == 0) return;

    std::vector<std::uint8_t> datagram;
    datagram.resize(mtu);

    for (std::uint16_t i = 0; i < chunkCount; ++i) {
        const std::size_t off = static_cast<std::size_t>(i) * maxPayload;
        const std::size_t remaining = jpeg.size() - off;
        const std::size_t payload = (remaining > maxPayload) ? maxPayload : remaining;

        const auto hdr = makeUdpHeader(
            frameId,
            frame_timestamp_us,
            i,
            chunkCount,
            jpegBytes,
            static_cast<std::uint32_t>(off),
            static_cast<std::uint16_t>(payload));

        std::memcpy(datagram.data(), &hdr, sizeof(hdr));
        std::memcpy(datagram.data() + sizeof(hdr), jpeg.data() + off, payload);

        const int toSend = static_cast<int>(sizeof(hdr) + payload);
        (void)sendto(sock_->sock,
                     reinterpret_cast<const char*>(datagram.data()),
                     toSend,
                     0,
                     reinterpret_cast<const sockaddr*>(&sock_->dest),
                     sizeof(sock_->dest));
    }
}

} // namespace sancak::net
