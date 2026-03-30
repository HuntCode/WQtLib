#pragma once

#include "IESServerCallback.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace hhcast {

class ESSession;
class ESPortManager;

class ESServer {
public:
    ESServer();
    ~ESServer();

    int StartServer();
    int StopServer();

    void SetCallback(std::shared_ptr<IESServerCallback> callback);

    bool IsRunning() const;

private:
    friend class ESPortManager;

    void OnTcpConnected(uint16_t localPort, const std::string& peerIp);
    void OnTcpDisconnected(uint16_t localPort, const std::string& peerIp);
    std::string HandleTcpRequest(uint16_t localPort, const std::string& peerIp, const std::string& request);

    void OnTcpData(uint16_t localPort, const std::string& peerIp, const uint8_t* data, size_t size);
    void OnUdpData(uint16_t localPort, const std::string& peerIp, const uint8_t* data, size_t size);

    std::shared_ptr<ESSession> GetSession(uint32_t streamId);
    std::shared_ptr<ESSession> CreateSession(uint32_t streamId);
    void RemoveSession(uint32_t streamId);
    void ClearSessions();

private:
    std::atomic<bool> m_running{ false };
    std::shared_ptr<IESServerCallback> m_callback = nullptr;

    std::unique_ptr<ESPortManager> m_portManager;
    std::unordered_map<uint32_t, std::shared_ptr<ESSession>> m_sessions;
};

} // namespace hhcast