#include "ESPortManager.h"

#include "ESServer.h"

#include <iostream>
#include <string>

namespace hhcast {

namespace {

static std::string ExtractPeerIp(const std::string& peerAddr)
{
    size_t pos = peerAddr.find(':');
    if (pos == std::string::npos) {
        return peerAddr;
    }

    return peerAddr.substr(0, pos);
}

} // namespace

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

    int ret = StartTcpServer(8700, m_tcpServer8700);
    if (ret != 0) {
        return ret;
    }

    ret = StartTcpServer(8121, m_tcpServer8121);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        return ret;
    }

    ret = StartTcpServer(57395, m_tcpServer57395);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        return ret;
    }

    m_running = true;
    return 0;
}

int ESPortManager::Stop()
{
    if (!m_running.load()) {
        return 0;
    }

    StopTcpServer(m_tcpServer8700);
    StopTcpServer(m_tcpServer8121);
    StopTcpServer(m_tcpServer57395);

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

int ESPortManager::StartTcpServer(uint16_t localPort, std::unique_ptr<hv::TcpServer>& server)
{
    server = std::make_unique<hv::TcpServer>();

    int listenfd = server->createsocket(localPort);
    if (listenfd < 0) {
        std::cout << "[ESPortManager] create tcp " << localPort << " socket failed" << std::endl;
        server.reset();
        return -100 - static_cast<int>(localPort);
    }

    server->onConnection = [this, localPort](const hv::SocketChannelPtr& channel) {
        std::string peerIp = ExtractPeerIp(channel->peeraddr());

        if (channel->isConnected()) {
            if (m_server) {
                m_server->OnTcpConnected(localPort, peerIp);
            }
        } else {
            if (m_server) {
                m_server->OnTcpDisconnected(localPort, peerIp);
            }
        }
    };

    server->onMessage = [this, localPort](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        if (m_server == nullptr || buf == nullptr || buf->size() == 0) {
            return;
        }

        std::string peerIp = ExtractPeerIp(channel->peeraddr());
        std::string request(reinterpret_cast<const char*>(buf->data()), buf->size());

        std::string response = m_server->HandleTcpRequest(localPort, peerIp, request);
        if (!response.empty()) {
            channel->write(response);
        }
    };

    server->setThreadNum(1);
    server->start();

    std::cout << "[ESPortManager] tcp " << localPort << " listening, fd=" << listenfd << std::endl;
    return 0;
}

void ESPortManager::StopTcpServer(std::unique_ptr<hv::TcpServer>& server)
{
    if (server) {
        server->stop();
        server.reset();
    }
}

} // namespace hhcast