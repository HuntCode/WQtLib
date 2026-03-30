#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace hhcast {

struct ESAudioPayloadInfo {
    uint8_t payloadType = 0;
    uint16_t sequence = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
};

using ESAudioPayloadCallback = std::function<void(const ESAudioPayloadInfo& info)>;

class ESAudioRtpParser {
public:
    ESAudioRtpParser();
    ~ESAudioRtpParser();

    void SetCallback(ESAudioPayloadCallback callback);

    bool ParsePacket(const uint8_t* data, size_t size);
    void Reset();

    uint64_t GetPacketCount() const;
    uint64_t GetDroppedPacketCount() const;
    uint64_t GetInputBytes() const;
    uint64_t GetPayloadBytes() const;

private:
    static uint16_t ReadBe16(const uint8_t* p);
    static uint32_t ReadBe32(const uint8_t* p);

private:
    ESAudioPayloadCallback m_callback;

    uint64_t m_packetCount = 0;
    uint64_t m_droppedPacketCount = 0;
    uint64_t m_inputBytes = 0;
    uint64_t m_payloadBytes = 0;
};

} // namespace hhcast