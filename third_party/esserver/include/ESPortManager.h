#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include "hv/TcpServer.h"

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

private:
    int StartTcpServer(uint16_t localPort, std::unique_ptr<hv::TcpServer>& server);
    void StopTcpServer(std::unique_ptr<hv::TcpServer>& server);

private:
    ESServer* m_server = nullptr;
    std::atomic<bool> m_running{ false };

    std::unique_ptr<hv::TcpServer> m_tcpServer8700;
    std::unique_ptr<hv::TcpServer> m_tcpServer8121;
};

} // namespace hhcast