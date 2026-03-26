#include "ESPortManager.h"

#include "ESServer.h"
#include "hv/TcpServer.h"
#include "hv/UdpServer.h"

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

    // TODO:
    // 1. 在这里逐步补充固定端口监听
    //    例如 51040 / 51030 / 8700 / 8121 / 8600 / 52020 / 52025 / 52030 / 57395
    // 2. 为 TCP/UDP 端口注册回调
    // 3. 收到连接或数据后，再转发给 m_server

    // 当前先保留最小 libhv 验证
    hv::TcpServer tcpServer;
    hv::UdpServer udpServer;

    m_running = true;
    return 0;
}

int ESPortManager::Stop()
{
    if (!m_running.load()) {
        return 0;
    }

    // TODO:
    // 1. 停止所有 TCP/UDP 监听
    // 2. 关闭相关资源

    m_running = false;
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