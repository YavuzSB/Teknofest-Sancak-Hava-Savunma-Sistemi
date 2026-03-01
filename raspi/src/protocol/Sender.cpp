// ÖRNEK: Raspberry Pi 5 tarafında UART üzerinden paket gönderimi
// Bu dosya bir "örnek" olarak projeye eklenmiştir.
// Uygulamanın mevcut TurretController/SerialComm ASCII protokolünü bozmaz.

#include "protocol/PacketBuilder.hpp"

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sancak::protocol {

class AsyncUartSender {
public:
    AsyncUartSender() = default;
    ~AsyncUartSender() { stop(); }

    AsyncUartSender(const AsyncUartSender&) = delete;
    AsyncUartSender& operator=(const AsyncUartSender&) = delete;

    bool start(const char* port, int baud);
    void stop();

    void enqueue(std::vector<uint8_t> frame);

private:
    void workerLoop();

#ifdef __linux__
    int fd_ = -1;
#endif

    std::atomic<bool> running_{false};
    std::thread worker_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::vector<uint8_t>> q_;
};

bool AsyncUartSender::start(const char* port, int baud) {
#ifdef __linux__
    (void)baud;
    stop();

    fd_ = ::open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return false;

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Şimdilik 115200 sabit (projede zaten bu hız kullanılıyor). İstenirse map eklenir.
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);

    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    tcflush(fd_, TCIFLUSH);
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    running_.store(true);
    worker_ = std::thread(&AsyncUartSender::workerLoop, this);
    return true;
#else
    (void)port;
    (void)baud;
    return false;
#endif
}

void AsyncUartSender::stop() {
    if (!running_.exchange(false)) {
        if (worker_.joinable()) worker_.join();
        return;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();

#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif

    std::lock_guard<std::mutex> lock(mtx_);
    while (!q_.empty()) q_.pop();
}

void AsyncUartSender::enqueue(std::vector<uint8_t> frame) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        q_.push(std::move(frame));
    }
    cv_.notify_one();
}

void AsyncUartSender::workerLoop() {
#ifdef __linux__
    while (running_.load()) {
        std::vector<uint8_t> frame;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&] { return !running_.load() || !q_.empty(); });
            if (!running_.load()) break;
            frame = std::move(q_.front());
            q_.pop();
        }

        if (fd_ < 0) continue;
        const uint8_t* p = frame.data();
        size_t left = frame.size();
        while (left > 0 && running_.load()) {
            const ssize_t n = ::write(fd_, p, left);
            if (n > 0) {
                p += static_cast<size_t>(n);
                left -= static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            break;
        }
    }
#endif
}

// Örnek kullanım:
// PacketBuilder pb;
// AimPayload a{pan, tilt, dist};
// auto frame = pb.build(MsgType::Aim, a);
// sender.enqueue(std::move(frame));

} // namespace sancak::protocol
