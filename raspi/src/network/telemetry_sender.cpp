#include "network/telemetry_sender.hpp"

#include <chrono>
#include <cstring>

#ifdef _WIN32
// Bu sınıf Raspberry Pi (Linux/POSIX) hedeflidir.
// Windows derlemelerinde link/başlık bağımlılığı oluşturmamak için no-op tutulur.
#else
#    include <arpa/inet.h>
#    include <errno.h>
#    include <fcntl.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>

namespace {
constexpr int kInvalidSocket = -1;

static int closeSocket(int s) {
    if (s == kInvalidSocket) return 0;
    return ::close(s);
}

static bool setNonBlocking(int sock, bool enabled) {
    const int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    const int next = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(sock, F_SETFL, next) == 0;
}

static bool connectWithTimeout(int sock, const sockaddr* addr, socklen_t addrlen, std::chrono::milliseconds timeout) {
    if (!setNonBlocking(sock, true)) return false;

    const int rc = ::connect(sock, addr, addrlen);
    if (rc == 0) {
        setNonBlocking(sock, false);
        return true;
    }

    if (errno != EINPROGRESS) {
        setNonBlocking(sock, false);
        return false;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);

    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    const int sel = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
    if (sel <= 0) {
        setNonBlocking(sock, false);
        return false;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (::getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) {
        setNonBlocking(sock, false);
        return false;
    }

    setNonBlocking(sock, false);
    return so_error == 0;
}

static bool resolveAndConnect(int sock, const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        return false;
    }

    bool ok = false;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        if (connectWithTimeout(sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen), std::chrono::milliseconds(1000))) {
            ok = true;
            break;
        }
    }

    ::freeaddrinfo(res);
    return ok;
}
} // namespace
#endif

namespace sancak::net {

TelemetrySender::TelemetrySender() = default;

TelemetrySender::~TelemetrySender() {
    stop();
}

bool TelemetrySender::start(std::string ip, std::uint16_t port) {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return true;
    }

    ip_ = std::move(ip);
    port_ = port;

    worker_ = std::thread(&TelemetrySender::workerLoop, this);
    return true;
}

void TelemetrySender::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    disconnect();
}

void TelemetrySender::sendTelemetry(const AimTelemetry& data) {
    if (!running_.load(std::memory_order_acquire)) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_queue_) {
            // backpressure: en eskiyi düşür
            queue_.pop_front();
        }
        queue_.push_back(data);
    }
    cv_.notify_one();
}

std::vector<std::uint8_t> TelemetrySender::buildAimTelemetryFrame(const AimTelemetry& t) {
    // Payload (V1 / 28 bytes):
    // u8  current_state
    // u8  reserved[3]
    // i32 target_id
    // f32 raw_x, raw_y
    // f32 corrected_x, corrected_y
    // f32 distance_m
    constexpr std::uint16_t kPayloadBytes = 28;

    std::vector<std::uint8_t> payload;
    payload.reserve(kPayloadBytes);
    payload.push_back(t.current_state);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);

    appendI32LE(payload, t.target_id);
    appendF32LE(payload, t.raw_x);
    appendF32LE(payload, t.raw_y);
    appendF32LE(payload, t.corrected_x);
    appendF32LE(payload, t.corrected_y);
    appendF32LE(payload, t.distance_m);

    const auto hdr = makeTcpHeader(TcpMsgType::kAimResultV1, kPayloadBytes);

    std::vector<std::uint8_t> frame;
    frame.reserve(sizeof(TcpMsgHeader) + payload.size());
    appendBytes(frame, &hdr, sizeof(hdr));
    appendBytes(frame, payload.data(), payload.size());
    return frame;
}

bool TelemetrySender::sendAll(int sock, const std::uint8_t* data, std::size_t size) {
#ifdef _WIN32
    (void)sock;
    (void)data;
    (void)size;
    return false;
#else
    std::size_t sentTotal = 0;
    while (sentTotal < size) {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        const ssize_t sent = ::send(sock,
                                   data + sentTotal,
                                   size - sentTotal,
                                   flags);
        if (sent > 0) {
            sentTotal += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent == 0) return false;
        if (errno == EINTR) continue;
        return false;
    }
    return true;
#endif
}

bool TelemetrySender::ensureConnected() {
#ifdef _WIN32
    return false;
#else
    if (sock_ != kInvalidSocket) return true;

    const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kInvalidSocket) {
        return false;
    }

    // timeouts: 500ms
    timeval tv{0, 500'000};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (!resolveAndConnect(s, ip_, port_)) {
        closeSocket(s);
        return false;
    }

    sock_ = s;
    return true;
#endif
}

void TelemetrySender::disconnect() {
#ifdef _WIN32
    sock_ = -1;
#else
    if (sock_ != kInvalidSocket) {
        closeSocket(sock_);
        sock_ = kInvalidSocket;
    }
#endif
}

void TelemetrySender::workerLoop() {
    using namespace std::chrono_literals;

    while (running_.load(std::memory_order_acquire)) {
#ifdef _WIN32
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 1s, [&] { return !running_.load(std::memory_order_acquire); });
        continue;
#else
        if (!ensureConnected()) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, 1s, [&] { return !running_.load(std::memory_order_acquire); });
            continue;
        }

        AimTelemetry item;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, 200ms, [&] {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });
            if (!running_.load(std::memory_order_acquire)) break;
            if (queue_.empty()) continue;
            item = queue_.front();
            queue_.pop_front();
        }

        const auto frame = buildAimTelemetryFrame(item);
        if (!sendAll(sock_, frame.data(), frame.size())) {
            disconnect();
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, 1s, [&] { return !running_.load(std::memory_order_acquire); });
            continue;
        }
#endif
    }

    // shutdown
    std::lock_guard<std::mutex> lock(mutex_);
    disconnect();
}

} // namespace sancak::net
