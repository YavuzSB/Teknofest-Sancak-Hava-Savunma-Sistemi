#include "protocol/PacketBuilder.hpp"

#include <cstring>

namespace sancak::protocol {

std::vector<uint8_t> PacketBuilder::buildRaw(MsgType type,
                                             const uint8_t* payload,
                                             uint8_t payloadLen) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint8_t> packet;
    packet.reserve(static_cast<size_t>(2 + 1 + 1 + payloadLen + 2));

    // Prefix
    packet.push_back(UART_H1);
    packet.push_back(UART_H2);

    packet.push_back(static_cast<uint8_t>(type));
    packet.push_back(payloadLen);

    if (payloadLen > 0 && payload != nullptr) {
        packet.insert(packet.end(), payload, payload + payloadLen);
    }

    const uint16_t crc = crc16_ccitt_false(packet.data(), packet.size());
    packet.push_back(static_cast<uint8_t>((crc >> 8) & 0xFFu));
    packet.push_back(static_cast<uint8_t>(crc & 0xFFu));

    return packet;
}

} // namespace sancak::protocol
