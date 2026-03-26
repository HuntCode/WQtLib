#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "hv/TcpServer.h"
#include "hv/UdpServer.h"

namespace hhcast {

class ESServer;

class ESPortManager {
public:
    ESPortManager();
    ~ESPortManager();

    int Start();
    int Stop();

    void SetServer(ESServer* server);

    bool IsRunning() const;

    uint16_t GetVideoPort() const;
    uint16_t GetMousePort() const;
    uint16_t GetDataPort() const;
    uint16_t GetControlPort() const;

private:
    int StartTcpServer(uint16_t localPort, std::unique_ptr<hv::TcpServer>& server);
    void StopTcpServer(std::unique_ptr<hv::TcpServer>& server);

    int StartUdpServer(uint16_t bindPort,
                       std::unique_ptr<hv::UdpServer>& server,
                       uint16_t& actualPort);
    void StopUdpServer(std::unique_ptr<hv::UdpServer>& server);

    void HandleTcpMessage(uint16_t localPort,
                          const hv::SocketChannelPtr& channel,
                          hv::Buffer* buf);

    void HandleUdpMessage(uint16_t localPort,
                          const hv::SocketChannelPtr& channel,
                          hv::Buffer* buf);

private:
    ESServer* m_server = nullptr;
    std::atomic<bool> m_running{ false };

    uint16_t m_videoPort = 51030;
    uint16_t m_mousePort = 51050;
    uint16_t m_dataPort = 0;
    uint16_t m_controlPort = 0;

    std::unique_ptr<hv::TcpServer> m_tcpServer8700;
    std::unique_ptr<hv::TcpServer> m_tcpServer8121;
    std::unique_ptr<hv::TcpServer> m_tcpServer57395;
    std::unique_ptr<hv::TcpServer> m_tcpServer8600;
    std::unique_ptr<hv::TcpServer> m_tcpServer51030;
    std::unique_ptr<hv::TcpServer> m_tcpServer51040;

    std::unique_ptr<hv::TcpServer> m_tcpServer52020;
    std::unique_ptr<hv::TcpServer> m_tcpServer52025;
    std::unique_ptr<hv::TcpServer> m_tcpServer52030;

    std::unique_ptr<hv::UdpServer> m_udpServer51050;
    std::unique_ptr<hv::UdpServer> m_udpServerDataPort;
    std::unique_ptr<hv::UdpServer> m_udpServerControlPort;

    std::unordered_map<std::string, std::string> m_tcpRecvBuffers8600;
    std::unordered_map<std::string, std::string> m_tcpRecvBuffers51040;
};

} // namespace hhcast