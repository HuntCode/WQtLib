#include "ESServer.h"

namespace hhcast {

ESServer::ESServer()
{
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

    m_running = true;
    return 0;
}

int ESServer::StopServer()
{
    if (!m_running.load()) {
        return 0;
    }

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

} // namespace hhcast