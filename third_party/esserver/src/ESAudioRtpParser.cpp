#include "ESAudioRtpParser.h"

namespace hhcast {

ESAudioRtpParser::ESAudioRtpParser() = default;
ESAudioRtpParser::~ESAudioRtpParser() = default;

void ESAudioRtpParser::SetCallback(ESAudioPayloadCallback callback)
{
    m_callback = std::move(callback);
}

bool ESAudioRtpParser::ParsePacket(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    ++m_packetCount;
    m_inputBytes += static_cast<uint64_t>(size);

    if (size < 12) {
        ++m_droppedPacketCount;
        return false;
    }

    const uint8_t version = (data[0] >> 6) & 0x03;
    const bool hasPadding = ((data[0] >> 5) & 0x01) != 0;
    const bool hasExtension = ((data[0] >> 4) & 0x01) != 0;
    const uint8_t csrcCount = data[0] & 0x0F;

    if (version != 2) {
        ++m_droppedPacketCount;
        return false;
    }

    size_t headerLen = 12 + static_cast<size_t>(csrcCount) * 4;
    if (size < headerLen) {
        ++m_droppedPacketCount;
        return false;
    }

    if (hasExtension) {
        if (size < headerLen + 4) {
            ++m_droppedPacketCount;
            return false;
        }

        const uint16_t extLenWords = ReadBe16(data + headerLen + 2);
        const size_t extBytes = 4 + static_cast<size_t>(extLenWords) * 4;
        if (size < headerLen + extBytes) {
            ++m_droppedPacketCount;
            return false;
        }

        headerLen += extBytes;
    }

    size_t paddingLen = 0;
    if (hasPadding) {
        paddingLen = static_cast<size_t>(data[size - 1]);
        if (paddingLen == 0 || size < headerLen + paddingLen) {
            ++m_droppedPacketCount;
            return false;
        }
    }

    const size_t payloadLen = size - headerLen - paddingLen;
    if (payloadLen == 0) {
        ++m_droppedPacketCount;
        return false;
    }

    ESAudioPayloadInfo info;
    info.payloadType = data[1] & 0x7F;
    info.sequence = ReadBe16(data + 2);
    info.timestamp = ReadBe32(data + 4);
    info.ssrc = ReadBe32(data + 8);
    info.payload = data + headerLen;
    info.payloadSize = payloadLen;

    m_payloadBytes += static_cast<uint64_t>(payloadLen);

    if (m_callback) {
        m_callback(info);
    }

    return true;
}

void ESAudioRtpParser::Reset()
{
    m_packetCount = 0;
    m_droppedPacketCount = 0;
    m_inputBytes = 0;
    m_payloadBytes = 0;
}

uint64_t ESAudioRtpParser::GetPacketCount() const
{
    return m_packetCount;
}

uint64_t ESAudioRtpParser::GetDroppedPacketCount() const
{
    return m_droppedPacketCount;
}

uint64_t ESAudioRtpParser::GetInputBytes() const
{
    return m_inputBytes;
}

uint64_t ESAudioRtpParser::GetPayloadBytes() const
{
    return m_payloadBytes;
}

uint16_t ESAudioRtpParser::ReadBe16(const uint8_t* p)
{
    return (static_cast<uint16_t>(p[0]) << 8) |
           (static_cast<uint16_t>(p[1]));
}

uint32_t ESAudioRtpParser::ReadBe32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           (static_cast<uint32_t>(p[3]));
}

} // namespace hhcast