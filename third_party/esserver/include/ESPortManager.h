#pragma once

#include <atomic>
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
    ESServer* m_server = nullptr;
    std::atomic<bool> m_running{ false };

    std::unique_ptr<hv::TcpServer> m_tcpServer8700;
};

} // namespace hhcast