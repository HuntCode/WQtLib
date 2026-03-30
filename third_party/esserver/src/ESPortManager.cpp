#include "ESPortManager.h"

#include "ESRtspLite.h"
#include "ESServer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

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

static uint16_t GetLocalPortByFd(int fd)
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));

    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return 0;
    }

    return ntohs(addr.sin_port);
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
    if (ret != 0) return ret;

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

    ret = StartTcpServer(m_videoPort, m_tcpServer51030);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        return ret;
    }

    ret = StartTcpServer(51040, m_tcpServer51040);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        return ret;
    }

    ret = StartTcpServer(52020, m_tcpServer52020);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        return ret;
    }

    ret = StartTcpServer(52025, m_tcpServer52025);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        StopTcpServer(m_tcpServer52020);
        return ret;
    }

    ret = StartTcpServer(52030, m_tcpServer52030);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        StopTcpServer(m_tcpServer52020);
        StopTcpServer(m_tcpServer52025);
        return ret;
    }

    ret = StartUdpServer(m_mousePort, m_udpServer51050, m_mousePort);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        StopTcpServer(m_tcpServer52020);
        StopTcpServer(m_tcpServer52025);
        StopTcpServer(m_tcpServer52030);
        return ret;
    }

    ret = StartUdpServer(0, m_udpServerDataPort, m_dataPort);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        StopTcpServer(m_tcpServer52020);
        StopTcpServer(m_tcpServer52025);
        StopTcpServer(m_tcpServer52030);
        StopUdpServer(m_udpServer51050);
        return ret;
    }

    ret = StartUdpServer(0, m_udpServerControlPort, m_controlPort);
    if (ret != 0) {
        StopTcpServer(m_tcpServer8700);
        StopTcpServer(m_tcpServer8121);
        StopTcpServer(m_tcpServer57395);
        StopTcpServer(m_tcpServer8600);
        StopTcpServer(m_tcpServer51030);
        StopTcpServer(m_tcpServer51040);
        StopTcpServer(m_tcpServer52020);
        StopTcpServer(m_tcpServer52025);
        StopTcpServer(m_tcpServer52030);
        StopUdpServer(m_udpServer51050);
        StopUdpServer(m_udpServerDataPort);
        return ret;
    }

    m_running = true;

    std::cout << "[ESPortManager] udp ports ready, mousePort=" << m_mousePort
              << ", dataPort=" << m_dataPort
              << ", controlPort=" << m_controlPort << std::endl;

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
    StopTcpServer(m_tcpServer51030);
    StopTcpServer(m_tcpServer51040);
    StopTcpServer(m_tcpServer52020);
    StopTcpServer(m_tcpServer52025);
    StopTcpServer(m_tcpServer52030);

    StopUdpServer(m_udpServer51050);
    StopUdpServer(m_udpServerDataPort);
    StopUdpServer(m_udpServerControlPort);

    m_mousePort = 51050;
    m_dataPort = 0;
    m_controlPort = 0;

    m_tcpRecvBuffers8600.clear();
    m_tcpRecvBuffers51040.clear();

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

uint16_t ESPortManager::GetVideoPort() const
{
    return m_videoPort;
}

uint16_t ESPortManager::GetMousePort() const
{
    return m_mousePort;
}

uint16_t ESPortManager::GetDataPort() const
{
    return m_dataPort;
}

uint16_t ESPortManager::GetControlPort() const
{
    return m_controlPort;
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
            if (localPort == 51040) {
                m_tcpRecvBuffers51040.erase(peerAddr);
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

int ESPortManager::StartUdpServer(uint16_t bindPort,
                                  std::unique_ptr<hv::UdpServer>& server,
                                  uint16_t& actualPort)
{
    server = std::make_unique<hv::UdpServer>();

    int sockfd = server->createsocket(bindPort);
    if (sockfd < 0) {
        std::cout << "[ESPortManager] create udp " << bindPort << " socket failed" << std::endl;
        server.reset();
        return -200 - static_cast<int>(bindPort);
    }

    actualPort = GetLocalPortByFd(sockfd);
    if (actualPort == 0) {
        std::cout << "[ESPortManager] get udp local port failed, bindPort=" << bindPort << std::endl;
        server.reset();
        return -300 - static_cast<int>(bindPort);
    }

    server->onMessage = [this, actualPort](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        HandleUdpMessage(actualPort, channel, buf);
    };

    server->start();

    std::cout << "[ESPortManager] udp " << actualPort << " listening, fd=" << sockfd << std::endl;
    return 0;
}

void ESPortManager::StopUdpServer(std::unique_ptr<hv::UdpServer>& server)
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

    if (localPort == 51030) {
        std::cout << "[ESPortManager][TCP][51030] recv " << buf->size()
        << " bytes from " << peerIp << std::endl;

        m_server->OnTcpData(
            localPort,
            peerIp,
            reinterpret_cast<const uint8_t*>(buf->data()),
            static_cast<size_t>(buf->size()));
        return;
    }

    if (localPort == 52020 || localPort == 52025 || localPort == 52030) {
        std::cout << "[ESPortManager][TCP][" << localPort << "] recv " << buf->size()
        << " bytes from " << peerIp << std::endl;
        return;
    }

    if (localPort == 51040) {
        std::string& cache = m_tcpRecvBuffers51040[peerAddr];
        cache += chunk;

        while (true) {
            ESRtspLiteMessage msg;
            std::string rawMsg;
            std::string error;
            if (!ESRtspLiteCodec::TryDecode(cache, msg, &rawMsg, &error)) {
                break;
            }

            std::string response = m_server->HandleTcpRequest(localPort, peerIp, rawMsg);
            if (!response.empty()) {
                channel->write(response);
            }
        }
        return;
    }

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

            if (cmdAvailability.rfind(cache, 0) == 0 || cmdState.rfind(cache, 0) == 0) {
                break;
            }

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

            cache.clear();
            break;
        }

        return;
    }

    std::string response = m_server->HandleTcpRequest(localPort, peerIp, chunk);
    if (!response.empty()) {
        channel->write(response);
    }
}

void ESPortManager::HandleUdpMessage(uint16_t localPort,
                                     const hv::SocketChannelPtr& channel,
                                     hv::Buffer* buf)
{
    if (m_server == nullptr || channel == nullptr || buf == nullptr || buf->size() == 0) {
        return;
    }

    std::string peerIp = ExtractPeerIp(channel->peeraddr());
    m_server->OnUdpData(localPort,
                        peerIp,
                        reinterpret_cast<const uint8_t*>(buf->data()),
                        static_cast<size_t>(buf->size()));
}

} // namespace hhcast