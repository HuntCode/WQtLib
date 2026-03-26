#include "ESSession.h"

namespace hhcast {

ESSession::ESSession(uint32_t streamId)
    : m_streamId(streamId)
{
}

ESSession::~ESSession()
{
}

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

} // namespace hhcast