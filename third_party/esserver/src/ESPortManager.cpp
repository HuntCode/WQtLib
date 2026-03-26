#include "ESPortManager.h"

#include "ESServer.h"

#include <iostream>
#include <string>
#include <algorithm>

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

static void TrimLeadingCrlf(std::string& text)
{
    while (!text.empty() && (text[0] == '\r' || text[0] == '\n')) {
        text.erase(text.begin());
    }
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

    ret = StartTcpServer(8600, m_tcpServer8600);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
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
    StopTcpServer(m_tcpServer8600);

    m_tcpRecvBuffers8600.clear();

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
        std::string peerAddr = channel->peeraddr();
        std::string peerIp = ExtractPeerIp(peerAddr);

        if (channel->isConnected()) {
            if (m_server) {
                m_server->OnTcpConnected(localPort, peerIp);
            }
        } else {
            if (localPort == 8600) {
                m_tcpRecvBuffers8600.erase(peerAddr);
            }

            if (m_server) {
                m_server->OnTcpDisconnected(localPort, peerIp);
            }
        }
    };

    server->onMessage = [this, localPort](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        HandleTcpMessage(localPort, channel, buf);
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

void ESPortManager::HandleTcpMessage(uint16_t localPort,
                                     const hv::SocketChannelPtr& channel,
                                     hv::Buffer* buf)
{
    if (m_server == nullptr || channel == nullptr || buf == nullptr || buf->size() == 0) {
        return;
    }

    std::string peerAddr = channel->peeraddr();
    std::string peerIp = ExtractPeerIp(peerAddr);
    std::string chunk(reinterpret_cast<const char*>(buf->data()), buf->size());

    // 8600 需要简单按连接缓存，因为命令可能拆包、补 CRLF、或者连续拼接
    if (localPort == 8600) {
        std::string& cache = m_tcpRecvBuffers8600[peerAddr];
        cache += chunk;

        const std::string cmdAvailability = "CameraAvailabilityCheck";
        const std::string cmdState = "CameraStateCheck";

        while (true) {
            TrimLeadingCrlf(cache);
            if (cache.empty()) {
                break;
            }

            if (cache.rfind(cmdAvailability, 0) == 0) {
                cache.erase(0, cmdAvailability.size());

                std::string response = m_server->HandleTcpRequest(localPort, peerIp, cmdAvailability);
                if (!response.empty()) {
                    channel->write(response);
                }
                continue;
            }

            if (cache.rfind(cmdState, 0) == 0) {
                cache.erase(0, cmdState.size());

                std::string response = m_server->HandleTcpRequest(localPort, peerIp, cmdState);
                if (!response.empty()) {
                    channel->write(response);
                }
                continue;
            }

            // 如果当前缓存只是已知命令的前缀，继续等待更多数据
            if (cmdAvailability.rfind(cache, 0) == 0 || cmdState.rfind(cache, 0) == 0) {
                break;
            }

            // 尝试在中间找到下一个合法命令起点，丢掉前面的噪声
            size_t posAvailability = cache.find(cmdAvailability);
            size_t posState = cache.find(cmdState);
            size_t pos = std::string::npos;

            if (posAvailability != std::string::npos && posState != std::string::npos) {
                pos = (std::min)(posAvailability, posState);
            } else if (posAvailability != std::string::npos) {
                pos = posAvailability;
            } else if (posState != std::string::npos) {
                pos = posState;
            }

            if (pos != std::string::npos && pos > 0) {
                cache.erase(0, pos);
                continue;
            }

            // 没找到合法命令，直接清空当前垃圾数据
            cache.clear();
            break;
        }

        return;
    }

    // 其他端口暂时沿用“一次 onMessage 就是一条完整请求”的简化模型
    std::string response = m_server->HandleTcpRequest(localPort, peerIp, chunk);
    if (!response.empty()) {
        channel->write(response);
    }
}

} // namespace hhcast