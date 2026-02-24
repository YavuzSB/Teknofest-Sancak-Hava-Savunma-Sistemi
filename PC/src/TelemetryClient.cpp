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
#include <vector>
#include <chrono>

#include "NetworkProtocol.hpp"

namespace teknofest {

// ── Ctor / Dtor ─────────────────────────────────────────────────────────────

TelemetryClient::TelemetryClient(const std::string& host, uint16_t port)
    : m_host(host)
    , m_port(port)
{}

TelemetryClient::~TelemetryClient() {
    stop();
}

void TelemetryClient::setLogCallback(LogCallback cb) {
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_logCallback = std::move(cb);
}

void TelemetryClient::emitLog(int level, const std::string& msg) {
    LogCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        cb = m_logCallback;
    }
    if (cb) cb(level, msg);
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

bool TelemetryClient::getLatestAimResult(AimResult& data) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_hasNewAim) return false;
    data = m_latestAim;
    m_hasNewAim = false;
    return true;
}

static uint32_t readU32LE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0])      ) |
           (static_cast<uint32_t>(p[1]) <<  8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static int32_t readI32LE(const uint8_t* p) {
    return static_cast<int32_t>(readU32LE(p));
}

static float readF32LE(const uint8_t* p) {
    uint32_t u = readU32LE(p);
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

bool TelemetryClient::tryParseAimFrame(std::vector<uint8_t>& buffer, AimResult& out) {
    if (buffer.size() < sizeof(teknofest::net::TcpMsgHeader)) return false;

    if (!teknofest::net::isTcpFrame(buffer.data(), buffer.size())) {
        // Magic mismatch (stream bozulması) -> 1 byte kaydırıp resync dene
        const auto now = std::chrono::steady_clock::now();
        if (m_lastParseWarn.time_since_epoch().count() == 0 ||
            (now - m_lastParseWarn) > std::chrono::seconds(1)) {
            emitLog(1, "Bozuk telemetri paketi alindi (Magic mismatch)!");
            m_lastParseWarn = now;
        }
        buffer.erase(buffer.begin());
        return false;
    }

    teknofest::net::TcpMsgHeader hdr{};
    std::memcpy(&hdr, buffer.data(), sizeof(hdr));

    if (hdr.version != teknofest::net::kProtocolVersion) {
        // magic match ama versiyon farklı: 1 byte kaydırıp tekrar dene
        const auto now = std::chrono::steady_clock::now();
        if (m_lastParseWarn.time_since_epoch().count() == 0 ||
            (now - m_lastParseWarn) > std::chrono::seconds(1)) {
            emitLog(1, "Bozuk telemetri paketi alindi (Version mismatch)!");
            m_lastParseWarn = now;
        }
        buffer.erase(buffer.begin());
        return false;
    }

    const size_t totalBytes = sizeof(hdr) + hdr.payload_bytes;
    if (buffer.size() < totalBytes) return false;

    if (hdr.type != static_cast<uint8_t>(teknofest::net::TcpMsgType::kAimResultV1) || hdr.payload_bytes < 28) {
        const auto now = std::chrono::steady_clock::now();
        if (m_lastParseWarn.time_since_epoch().count() == 0 ||
            (now - m_lastParseWarn) > std::chrono::seconds(1)) {
            emitLog(1, "Bozuk telemetri paketi alindi (Frame type/size)!");
            m_lastParseWarn = now;
        }
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(totalBytes));
        return false;
    }

    const uint8_t* p = buffer.data() + sizeof(hdr);
    out.frame_id = readU32LE(p + 0);
    out.raw_x = readF32LE(p + 4);
    out.raw_y = readF32LE(p + 8);
    out.corrected_x = readF32LE(p + 12);
    out.corrected_y = readF32LE(p + 16);
    out.class_id = readI32LE(p + 20);
    out.valid = (p[24] != 0);

    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(totalBytes));
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
        emitLog(0, "Sunucuya baglaniliyor: " + m_host + ":" + std::to_string(m_port));

        socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == kInvalidSocket) {
            emitLog(2, "Socket olusturulamadi");
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
            emitLog(1, "TCP baglanti denemesi basarisiz");
            m_state.store(State::Retrying, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(kBackoffMs));
            continue;
        }

        m_state.store(State::Connected, std::memory_order_release);
        emitLog(0, "TCP baglandi");

        // Okunma zaman aşımı (500 ms)
#ifdef _WIN32
        DWORD timeout = 500;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval tv { 0, 500'000 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        std::vector<uint8_t> recvBuffer;
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
                        emitLog(2, "TCP baglantisi koptu/hata olustu! (send)");
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
                recvBuffer.insert(recvBuffer.end(),
                                  reinterpret_cast<uint8_t*>(chunk),
                                  reinterpret_cast<uint8_t*>(chunk) + received);

                // Parse: Önce binary frame, sonra newline telemetry
                for (;;) {
                    AimResult aim{};
                    if (tryParseAimFrame(recvBuffer, aim)) {
                        std::lock_guard<std::mutex> lock(m_dataMutex);
                        m_latestAim = aim;
                        m_hasNewAim = true;
                        continue;
                    }

                    // newline ara
                    auto it = std::find(recvBuffer.begin(), recvBuffer.end(), static_cast<uint8_t>('\n'));
                    if (it == recvBuffer.end()) break;

                    const auto len = static_cast<size_t>(std::distance(recvBuffer.begin(), it));
                    std::string line(reinterpret_cast<const char*>(recvBuffer.data()), len);
                    recvBuffer.erase(recvBuffer.begin(), it + 1);

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

                // Safety: buffer büyümesin
                if (recvBuffer.size() > 1'000'000) {
                    emitLog(1, "Alim buffer temizlendi (cok buyudu)");
                    recvBuffer.clear();
                }
            } else if (received == 0) {
                // Bağlantı kapandı
                emitLog(2, "TCP baglantisi koptu/hata olustu! (peer closed)");
                connectionError = true;
            }
            // received < 0 → zaman aşımı, döngüye devam
        }

        closeSocket(sock);

        if (connectionError && m_running.load(std::memory_order_acquire)) {
            emitLog(1, "Telemetri yeniden baglaniyor...");
            m_state.store(State::Retrying, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(kBackoffMs));
        }
    }

    m_state.store(State::Stopped, std::memory_order_release);
}

} // namespace teknofest
