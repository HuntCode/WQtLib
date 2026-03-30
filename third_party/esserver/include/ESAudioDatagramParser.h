#pragma once

#include "ESAudioRtpParser.h"

#include <cstddef>
#include <cstdint>

namespace hhcast {

class ESAudioDatagramParser {
public:
    ESAudioDatagramParser();
    ~ESAudioDatagramParser();

    void SetCallback(ESAudioPayloadCallback callback);

    // 输入一个完整 UDP datagram，内部可能拆出多个 RTP 包
    bool ParseDatagram(const uint8_t* data, size_t size);

    void Reset();

    uint64_t GetDatagramCount() const;
    uint64_t GetDroppedDatagramCount() const;
    uint64_t GetSplitPacketCount() const;
    uint64_t GetInputBytes() const;

private:
    struct RtpHeaderLite {
        uint8_t version = 0;
        bool hasPadding = false;
        bool hasExtension = false;
        uint8_t csrcCount = 0;
        uint8_t payloadType = 0;
        uint16_t sequence = 0;
        uint32_t timestamp = 0;
        uint32_t ssrc = 0;
        size_t minHeaderLen = 12;
    };

    static bool ParseRtpHeaderLite(const uint8_t* data, size_t size, RtpHeaderLite& header);
    static bool IsNextSequence(uint16_t currentSeq, uint16_t nextSeq);

    size_t FindNextRtpOffset(const uint8_t* data,
                             size_t size,
                             size_t currentOffset,
                             const RtpHeaderLite& currentHeader) const;

private:
    ESAudioRtpParser m_rtpParser;

    uint64_t m_datagramCount = 0;
    uint64_t m_droppedDatagramCount = 0;
    uint64_t m_splitPacketCount = 0;   // 拆出来的 RTP 包总数
    uint64_t m_inputBytes = 0;
};

} // namespace hhcast