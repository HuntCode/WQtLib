#include "ESSession.h"

namespace hhcast {

ESSession::ESSession(uint32_t streamId)
    : m_streamId(streamId)
{
    m_videoDepacketizer.SetCallback(
        [this](const ESVideoUnit& unit) {
            OnVideoUnitReady(unit);
        });

    m_audioDatagramParser.SetCallback(
        [this](const ESAudioPayloadInfo& info) {
            OnAudioPayloadReady(info);
        });
}

ESSession::~ESSession() = default;

uint32_t ESSession::GetStreamId() const
{
    return m_streamId;
}

void ESSession::SetPeerIp(const std::string& peerIp)
{
    m_peerIp = peerIp;
}

const std::string& ESSession::GetPeerIp() const
{
    return m_peerIp;
}

void ESSession::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& ESSession::GetName() const
{
    return m_name;
}

void ESSession::SetVideoCallback(ESSessionVideoCallback callback)
{
    m_videoCallback = std::move(callback);
}

void ESSession::SetAudioCallback(ESSessionAudioCallback callback)
{
    m_audioCallback = std::move(callback);
}

bool ESSession::InputVideoTcpData(const uint8_t* data, size_t size)
{
    return m_videoDepacketizer.PushBytes(data, size);
}

bool ESSession::InputAudioUdpDatagram(const uint8_t* data, size_t size)
{
    return m_audioDatagramParser.ParseDatagram(data, size);
}

void ESSession::ResetMediaState()
{
    m_videoDepacketizer.Reset();
    m_audioDatagramParser.Reset();
}

void ESSession::OnVideoUnitReady(const ESVideoUnit& unit)
{
    if (!m_videoCallback || unit.payload == nullptr || unit.payloadSize == 0) {
        return;
    }

    m_videoCallback(m_streamId, unit.payload, unit.payloadSize, unit);
}

void ESSession::OnAudioPayloadReady(const ESAudioPayloadInfo& info)
{
    if (!m_audioCallback || info.payload == nullptr || info.payloadSize == 0) {
        return;
    }

    m_audioCallback(m_streamId, info.payload, info.payloadSize, info);
}

} // namespace hhcast