#pragma once

#include "ProtocolDef.h"

#include <cstdint>
#include <mutex>
#include <type_traits>
#include <vector>

namespace sancak::protocol {

class PacketBuilder {
public:
    PacketBuilder() = default;

    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;

    template <typename T>
    [[nodiscard]] std::vector<uint8_t> build(MsgType type, const T& payload) const {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Payload struct trivially copyable olmalı");
        static_assert(sizeof(T) <= 255, "PayloadSize 1 byte olduğu için <=255 olmalı");

        const auto* bytes = reinterpret_cast<const uint8_t*>(&payload);
        return buildRaw(type, bytes, static_cast<uint8_t>(sizeof(T)));
    }

    [[nodiscard]] std::vector<uint8_t> buildRaw(MsgType type,
                                                const uint8_t* payload,
                                                uint8_t payloadLen) const;

private:
    mutable std::mutex mutex_;
};

} // namespace sancak::protocol
