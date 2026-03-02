#include "network/tcp_telemetry_server.hpp"

#include <chrono>
#include <cstring>

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

struct TcpTelemetryServer::SocketHandle {
    socket_t sock = kInvalidSocket;
};

TcpTelemetryServer::TcpTelemetryServer() = default;

TcpTelemetryServer::~TcpTelemetryServer() {
    stop();
}

bool TcpTelemetryServer::start(TcpTelemetryConfig cfg) {
    if (running_.load(std::memory_order_acquire)) return true;
    cfg_ = cfg;
    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&TcpTelemetryServer::serverLoop, this);
    return true;
}

void TcpTelemetryServer::stop() {
    running_.store(false, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }

    if (listen_sock_) {
        closeSocket(listen_sock_->sock);
        delete listen_sock_;
        listen_sock_ = nullptr;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    std::lock_guard<std::mutex> lock(out_mutex_);
    while (!out_queue_.empty()) out_queue_.pop();

    {
        std::lock_guard<std::mutex> lockCmd(cmd_mutex_);
        cmd_queue_.clear();
    }
}

bool TcpTelemetryServer::tryPopCommand(std::string& outCommand) {
    std::lock_guard<std::mutex> lock(cmd_mutex_);
    if (cmd_queue_.empty()) return false;
    outCommand = std::move(cmd_queue_.front());
    cmd_queue_.pop_front();
    return true;
}

void TcpTelemetryServer::enqueueCommand(std::string line) {
    // trim (whitespace/newline already removed)
    auto ltrim = [](std::string& s) {
        const auto b = s.find_first_not_of(" \t\r\n");
        s = (b == std::string::npos) ? "" : s.substr(b);
    };
    auto rtrim = [](std::string& s) {
        const auto e = s.find_last_not_of(" \t\r\n");
        s = (e == std::string::npos) ? "" : s.substr(0, e + 1);
    };
    ltrim(line);
    rtrim(line);
    if (line.empty()) return;

    std::lock_guard<std::mutex> lock(cmd_mutex_);
    if (cmd_queue_.size() >= max_cmd_queue_) {
        // backpressure: en eski komutları düşür.
        while (cmd_queue_.size() >= max_cmd_queue_) {
            cmd_queue_.pop_front();
        }
    }
    cmd_queue_.push_back(std::move(line));
}

void TcpTelemetryServer::publishAimResult(const sancak::AimResult& aim, std::uint32_t frame_id) {
    if (!running_.load(std::memory_order_acquire)) return;
    enqueueFrame(buildAimFrame(aim, frame_id));
}

void TcpTelemetryServer::enqueueFrame(std::vector<std::uint8_t> frame) {
    std::lock_guard<std::mutex> lock(out_mutex_);
    // backpressure: kuyruk büyümesin, sadece en son mesajı tut.
    while (!out_queue_.empty()) out_queue_.pop();
    out_queue_.push(std::move(frame));
}

std::vector<std::uint8_t> TcpTelemetryServer::buildAimFrame(const sancak::AimResult& aim,
                                                            std::uint32_t frame_id)
{
    // Payload (V1):
    // u32 frame_id
    // f32 raw_x, raw_y
    // f32 corr_x, corr_y
    // i32 class_id
    // u8  valid
    // u8  reserved[3]
    constexpr std::uint16_t kPayloadBytes = 28;
    std::vector<std::uint8_t> payload;
    payload.reserve(kPayloadBytes);
    appendU32LE(payload, frame_id);
    appendF32LE(payload, aim.raw_xy.x);
    appendF32LE(payload, aim.raw_xy.y);
    appendF32LE(payload, aim.corrected_xy.x);
    appendF32LE(payload, aim.corrected_xy.y);
    appendI32LE(payload, aim.class_id);
    payload.push_back(static_cast<std::uint8_t>(aim.valid ? 1 : 0));
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);

    const auto hdr = makeTcpHeader(TcpMsgType::kAimResultV1, kPayloadBytes);

    std::vector<std::uint8_t> frame;
    frame.reserve(sizeof(TcpMsgHeader) + payload.size());
    appendBytes(frame, &hdr, sizeof(hdr));
    appendBytes(frame, payload.data(), payload.size());
    return frame;
}

static bool sendAll(socket_t sock, const std::uint8_t* data, std::size_t size) {
    std::size_t sentTotal = 0;
    while (sentTotal < size) {
        const int sent = send(sock,
                              reinterpret_cast<const char*>(data + sentTotal),
                              static_cast<int>(size - sentTotal),
                              0);
        if (sent <= 0) return false;
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

void TcpTelemetryServer::serverLoop() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        running_.store(false, std::memory_order_release);
        return;
    }
#endif

    auto* listenH = new SocketHandle();
    listenH->sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenH->sock == kInvalidSocket) {
        delete listenH;
        running_.store(false, std::memory_order_release);
        return;
    }

    int yes = 1;
    setsockopt(listenH->sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg_.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(listenH->sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeSocket(listenH->sock);
        delete listenH;
        running_.store(false, std::memory_order_release);
        return;
    }
    if (::listen(listenH->sock, 1) < 0) {
        closeSocket(listenH->sock);
        delete listenH;
        running_.store(false, std::memory_order_release);
        return;
    }

    listen_sock_ = listenH;

    while (running_.load(std::memory_order_acquire)) {
        // accept (blocking) -> kısa timeout ile poll yapalım
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_sock_->sock, &rfds);
        timeval tv{0, 500'000};
        const int sel = select(static_cast<int>(listen_sock_->sock + 1), &rfds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            continue;
        }

        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        socket_t client = accept(listen_sock_->sock,
                                 reinterpret_cast<sockaddr*>(&clientAddr),
                                 &clientLen);
        if (client == kInvalidSocket) {
            continue;
        }

        // timeouts (500ms)
#ifdef _WIN32
        DWORD timeout = 500;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        timeval tv2{0, 500'000};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv2, sizeof(tv2));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv2, sizeof(tv2));
#endif

        std::string inLineBuf;
        bool connectionOk = true;

        while (running_.load(std::memory_order_acquire) && connectionOk) {
            // 1) outgoing (latest frame)
            std::optional<std::vector<std::uint8_t>> out;
            {
                std::lock_guard<std::mutex> lock(out_mutex_);
                if (!out_queue_.empty()) {
                    out = std::move(out_queue_.front());
                    out_queue_.pop();
                }
            }
            if (out.has_value()) {
                if (!sendAll(client, out->data(), out->size())) {
                    connectionOk = false;
                    break;
                }
            }

            // 2) consume incoming commands (newline delimited)
            char buf[1024];
            const int recvd = recv(client, buf, sizeof(buf), 0);
            if (recvd > 0) {
                inLineBuf.append(buf, buf + recvd);
                for (;;) {
                    const auto pos = inLineBuf.find('\n');
                    if (pos == std::string::npos) break;
                    std::string line = inLineBuf.substr(0, pos);
                    inLineBuf.erase(0, pos + 1);
                    enqueueCommand(std::move(line));
                }
            } else if (recvd == 0) {
                connectionOk = false;
                break;
            }
            // recvd < 0 => timeout
        }

        closeSocket(client);
    }
}

} // namespace sancak::net
