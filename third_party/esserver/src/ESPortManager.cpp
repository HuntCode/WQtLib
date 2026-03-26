#include "ESPortManager.h"
#include "ESServer.h"

#include <iostream>
#include <string>

namespace hhcast {

ESPortManager::ESPortManager()
{
}

ESPortManager::~ESPortManager()
{
    Stop();
}

int ESPortManager::Start()
{
    if (m_running.load()) {
        return 0;
    }

    if (m_server == nullptr) {
        std::cout << "[ESPortManager] server is null" << std::endl;
        return -1;
    }

    m_tcpServer8700 = std::make_unique<hv::TcpServer>();

    int listenfd = m_tcpServer8700->createsocket(8700);
    if (listenfd < 0) {
        std::cout << "[ESPortManager] create 8700 socket failed" << std::endl;
        m_tcpServer8700.reset();
        return -2;
    }

    m_tcpServer8700->onConnection = [this](const hv::SocketChannelPtr& channel) {
        std::string peerIp = channel->peeraddr();

        if (channel->isConnected()) {
            if (m_server) {
                m_server->OnTcpConnected(8700, peerIp);
            }
        } else {
            if (m_server) {
                m_server->OnTcpDisconnected(8700, peerIp);
            }
        }
    };

    m_tcpServer8700->onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        if (m_server == nullptr || buf == nullptr || buf->size() == 0) {
            return;
        }

        std::string peerIp = channel->peeraddr();
        std::string request(reinterpret_cast<const char*>(buf->data()), buf->size());

        std::string response = m_server->HandleTcpRequest(8700, peerIp, request);
        if (!response.empty()) {
            channel->write(response);
        }
    };

    m_tcpServer8700->setThreadNum(1);
    m_tcpServer8700->start();

    m_running = true;
    std::cout << "[ESPortManager] tcp 8700 listening, fd=" << listenfd << std::endl;

    return 0;
}

int ESPortManager::Stop()
{
    if (!m_running.load()) {
        return 0;
    }

    if (m_tcpServer8700) {
        m_tcpServer8700->stop();
        m_tcpServer8700.reset();
    }

    m_running = false;
    std::cout << "[ESPortManager] stopped" << std::endl;

    return 0;
}

void ESPortManager::SetServer(ESServer* server)
{
    m_server = server;
}

bool ESPortManager::IsRunning() const
{
    return m_running.load();
}

} // namespace hhcast