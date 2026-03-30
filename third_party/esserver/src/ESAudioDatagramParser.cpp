#include "ESAudioDatagramParser.h"

namespace hhcast {

namespace {
constexpr size_t kRtpFixedHeaderSize = 12;
constexpr size_t kMinCandidateRemainSize = 12;
} // namespace

ESAudioDatagramParser::ESAudioDatagramParser() = default;
ESAudioDatagramParser::~ESAudioDatagramParser() = default;

void ESAudioDatagramParser::SetCallback(ESAudioPayloadCallback callback)
{
    m_rtpParser.SetCallback(std::move(callback));
}

bool ESAudioDatagramParser::ParseDatagram(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    ++m_datagramCount;
    m_inputBytes += static_cast<uint64_t>(size);

    size_t currentOffset = 0;
    bool parsedAnyPacket = false;

    while (currentOffset < size) {
        const uint8_t* current = data + currentOffset;
        const size_t remain = size - currentOffset;

        RtpHeaderLite currentHeader;
        if (!ParseRtpHeaderLite(current, remain, currentHeader)) {
            ++m_droppedDatagramCount;
            return parsedAnyPacket;
        }

        const size_t nextOffset = FindNextRtpOffset(data, size, currentOffset, currentHeader);
        const size_t packetSize =
            (nextOffset == size) ? (size - currentOffset) : (nextOffset - currentOffset);

        if (packetSize < kRtpFixedHeaderSize) {
            ++m_droppedDatagramCount;
            return parsedAnyPacket;
        }

        if (!m_rtpParser.ParsePacket(current, packetSize)) {
            ++m_droppedDatagramCount;
            return parsedAnyPacket;
        }

        ++m_splitPacketCount;
        parsedAnyPacket = true;

        if (nextOffset == size) {
            break;
        }

        currentOffset = nextOffset;
    }

    return parsedAnyPacket;
}

void ESAudioDatagramParser::Reset()
{
    m_rtpParser.Reset();
    m_datagramCount = 0;
    m_droppedDatagramCount = 0;
    m_splitPacketCount = 0;
    m_inputBytes = 0;
}

uint64_t ESAudioDatagramParser::GetDatagramCount() const
{
    return m_datagramCount;
}

uint64_t ESAudioDatagramParser::GetDroppedDatagramCount() const
{
    return m_droppedDatagramCount;
}

uint64_t ESAudioDatagramParser::GetSplitPacketCount() const
{
    return m_splitPacketCount;
}

uint64_t ESAudioDatagramParser::GetInputBytes() const
{
    return m_inputBytes;
}

bool ESAudioDatagramParser::ParseRtpHeaderLite(const uint8_t* data, size_t size, RtpHeaderLite& header)
{
    if (data == nullptr || size < kRtpFixedHeaderSize) {
        return false;
    }

    const uint8_t b0 = data[0];
    const uint8_t b1 = data[1];

    header.version = (b0 >> 6) & 0x03;
    header.hasPadding = ((b0 >> 5) & 0x01) != 0;
    header.hasExtension = ((b0 >> 4) & 0x01) != 0;
    header.csrcCount = b0 & 0x0F;
    header.payloadType = b1 & 0x7F;
    header.sequence = (static_cast<uint16_t>(data[2]) << 8) |
                      (static_cast<uint16_t>(data[3]));
    header.timestamp = (static_cast<uint32_t>(data[4]) << 24) |
                       (static_cast<uint32_t>(data[5]) << 16) |
                       (static_cast<uint32_t>(data[6]) << 8)  |
                       (static_cast<uint32_t>(data[7]));
    header.ssrc = (static_cast<uint32_t>(data[8]) << 24) |
                  (static_cast<uint32_t>(data[9]) << 16) |
                  (static_cast<uint32_t>(data[10]) << 8) |
                  (static_cast<uint32_t>(data[11]));

    header.minHeaderLen = kRtpFixedHeaderSize + static_cast<size_t>(header.csrcCount) * 4;

    if (header.version != 2) {
        return false;
    }

    if (size < header.minHeaderLen) {
        return false;
    }

    return true;
}

bool ESAudioDatagramParser::IsNextSequence(uint16_t currentSeq, uint16_t nextSeq)
{
    return static_cast<uint16_t>(currentSeq + 1) == nextSeq;
}

size_t ESAudioDatagramParser::FindNextRtpOffset(const uint8_t* data,
                                                size_t size,
                                                size_t currentOffset,
                                                const RtpHeaderLite& currentHeader) const
{
    const size_t scanBegin = currentOffset + currentHeader.minHeaderLen;
    if (scanBegin + kMinCandidateRemainSize > size) {
        return size;
    }

    for (size_t pos = scanBegin; pos + kMinCandidateRemainSize <= size; ++pos) {
        RtpHeaderLite candidate;
        if (!ParseRtpHeaderLite(data + pos, size - pos, candidate)) {
            continue;
        }

        // 这几个条件用于降低把 payload 内字节误判成 RTP 头的概率
        if (candidate.payloadType != currentHeader.payloadType) {
            continue;
        }

        if (candidate.ssrc != currentHeader.ssrc) {
            continue;
        }

        if (!IsNextSequence(currentHeader.sequence, candidate.sequence)) {
            continue;
        }

        return pos;
    }

    return size;
}

} // namespace hhcast