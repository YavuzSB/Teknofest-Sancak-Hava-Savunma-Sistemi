/**
 * @file TelemetryClient.cpp
 * @brief TCP telemetri istemcisi – Winsock2 / POSIX soket tabanlı
 */
#include "TelemetryClient.hpp"

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

#include <algorithm>
#include <sstream>
#include <cstring>

namespace teknofest {

// ── Ctor / Dtor ─────────────────────────────────────────────────────────────

TelemetryClient::TelemetryClient(const std::string& host, uint16_t port)
    : m_host(host)
    , m_port(port)
{}

TelemetryClient::~TelemetryClient() {
    stop();
}

// ── Public API ──────────────────────────────────────────────────────────────

void TelemetryClient::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&TelemetryClient::clientLoop, this);
}

void TelemetryClient::stop() {
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_state.store(State::Stopped, std::memory_order_release);
}

void TelemetryClient::sendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    std::string cmd = command;
    if (cmd.empty() || cmd.back() != '\n') {
        cmd += '\n';
    }
    m_sendQueue.push(std::move(cmd));
}

bool TelemetryClient::getLatestTelemetry(TelemetryData& data) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_hasNewData) return false;
    data = m_latestData;
    m_hasNewData = false;
    return true;
}

// ── Parse ───────────────────────────────────────────────────────────────────

TelemetryData TelemetryClient::parseTelemetry(const std::string& line) {
    TelemetryData result;

    auto trim = [](std::string& s) {
        const auto begin = s.find_first_not_of(" \t\r\n");
        const auto end   = s.find_last_not_of(" \t\r\n");
        s = (begin == std::string::npos) ? "" : s.substr(begin, end - begin + 1);
    };

    std::istringstream ss(line);
    std::string part;

    while (std::getline(ss, part, ';')) {
        const auto eqPos = part.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = part.substr(0, eqPos);
        std::string val = part.substr(eqPos + 1);
        trim(key);
        trim(val);

        // Key → upper
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (key == "HIZ" || key == "SPEED") {
            result.speed = val;
        } else if (key == "YON" || key == "DIR" || key == "DIRECTION") {
            result.direction = val;
        } else if (key == "MOD" || key == "MODE") {
            result.mode = val;
        }
    }

    return result;
}

// ── İstemci thread döngüsü ─────────────────────────────────────────────────

void TelemetryClient::clientLoop() {
    constexpr int kBackoffMs = 2000;

    while (m_running.load(std::memory_order_acquire)) {
        m_state.store(State::Connecting, std::memory_order_release);

        socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == kInvalidSocket) {
            m_state.store(State::Retrying, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(kBackoffMs));
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(m_port);
        inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr);

        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            closeSocket(sock);
            m_state.store(State::Retrying, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(kBackoffMs));
            continue;
        }

        m_state.store(State::Connected, std::memory_order_release);

        // Okunma zaman aşımı (500 ms)
#ifdef _WIN32
        DWORD timeout = 500;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval tv { 0, 500'000 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        std::string recvBuffer;
        bool connectionError = false;

        while (m_running.load(std::memory_order_acquire) && !connectionError) {
            // ── Kuyruktaki komutları gönder ─────────────────────────────────
            {
                std::lock_guard<std::mutex> lock(m_sendMutex);
                while (!m_sendQueue.empty()) {
                    const std::string& cmd = m_sendQueue.front();
                    const int sent = send(sock, cmd.c_str(),
                                          static_cast<int>(cmd.size()), 0);
                    if (sent <= 0) {
                        connectionError = true;
                        break;
                    }
                    m_sendQueue.pop();
                }
            }
            if (connectionError) break;

            // ── Veri al ─────────────────────────────────────────────────────
            char chunk[4096];
            const int received = recv(sock, chunk, sizeof(chunk) - 1, 0);

            if (received > 0) {
                chunk[received] = '\0';
                recvBuffer += chunk;

                // Tam satırları işle
                std::string::size_type pos;
                while ((pos = recvBuffer.find('\n')) != std::string::npos) {
                    std::string line = recvBuffer.substr(0, pos);
                    recvBuffer.erase(0, pos + 1);

                    // Trim
                    {
                        const auto b = line.find_first_not_of(" \t\r\n");
                        const auto e = line.find_last_not_of(" \t\r\n");
                        line = (b == std::string::npos) ? "" : line.substr(b, e - b + 1);
                    }

                    if (!line.empty()) {
                        TelemetryData data = parseTelemetry(line);
                        std::lock_guard<std::mutex> lock(m_dataMutex);
                        m_latestData = data;
                        m_hasNewData = true;
                    }
                }
            } else if (received == 0) {
                // Bağlantı kapandı
                connectionError = true;
            }
            // received < 0 → zaman aşımı, döngüye devam
        }

        closeSocket(sock);

        if (connectionError && m_running.load(std::memory_order_acquire)) {
            m_state.store(State::Retrying, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(kBackoffMs));
        }
    }

    m_state.store(State::Stopped, std::memory_order_release);
}

} // namespace teknofest
