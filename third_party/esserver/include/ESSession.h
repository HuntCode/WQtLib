#pragma once

#include "ESAudioDatagramParser.h"
#include "ESVideoDepacketizer.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace hhcast {

using ESSessionVideoCallback = std::function<void(
    uint32_t streamId,
    const uint8_t* data,
    size_t size,
    const ESVideoUnit& unit)>;

using ESSessionAudioCallback = std::function<void(
    uint32_t streamId,
    const uint8_t* data,
    size_t size,
    const ESAudioPayloadInfo& info)>;

class ESSession {
public:
    explicit ESSession(uint32_t streamId);
    ~ESSession();

    uint32_t GetStreamId() const;

    void SetPeerIp(const std::string& peerIp);
    const std::string& GetPeerIp() const;

    void SetName(const std::string& name);
    const std::string& GetName() const;

    void SetVideoCallback(ESSessionVideoCallback callback);
    void SetAudioCallback(ESSessionAudioCallback callback);

    bool InputVideoTcpData(const uint8_t* data, size_t size);
    bool InputAudioUdpDatagram(const uint8_t* data, size_t size);

    void ResetMediaState();

private:
    void OnVideoUnitReady(const ESVideoUnit& unit);
    void OnAudioPayloadReady(const ESAudioPayloadInfo& info);

private:
    uint32_t m_streamId = 0;
    std::string m_peerIp;
    std::string m_name;

    ESVideoDepacketizer m_videoDepacketizer;
    ESAudioDatagramParser m_audioDatagramParser;

    ESSessionVideoCallback m_videoCallback;
    ESSessionAudioCallback m_audioCallback;
};

} // namespace hhcast