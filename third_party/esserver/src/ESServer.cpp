#include "ESServer.h"
#include "ESPortManager.h"
#include "ESSession.h"

namespace hhcast {

ESServer::ESServer()
{
    m_portManager = std::make_unique<ESPortManager>();
    m_portManager->SetServer(this);
}

ESServer::~ESServer()
{
    StopServer();
}

int ESServer::StartServer()
{
    if (m_running.load()) {
        return 0;
    }

    int ret = m_portManager->Start();
    if (ret != 0) {
        return ret;
    }

    m_running = true;
    return 0;
}

int ESServer::StopServer()
{
    if (!m_running.load()) {
        return 0;
    }

    m_portManager->Stop();

    ClearSessions();
    m_running = false;

    return 0;
}

void ESServer::SetCallback(std::shared_ptr<IESServerCallback> callback)
{
    m_callback = callback;
}

bool ESServer::IsRunning() const
{
    return m_running.load();
}

std::shared_ptr<ESSession> ESServer::GetSession(uint32_t streamId)
{
    auto it = m_sessions.find(streamId);
    if (it != m_sessions.end()) {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<ESSession> ESServer::CreateSession(uint32_t streamId)
{
    auto session = GetSession(streamId);
    if (session) {
        return session;
    }

    session = std::make_shared<ESSession>(streamId);
    m_sessions[streamId] = session;
    return session;
}

void ESServer::RemoveSession(uint32_t streamId)
{
    m_sessions.erase(streamId);
}

void ESServer::ClearSessions()
{
    m_sessions.clear();
}

} // namespace hhcast