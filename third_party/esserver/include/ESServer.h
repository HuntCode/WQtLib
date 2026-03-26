#pragma once

#include "IESServerCallback.h"

#include <atomic>
#include <cstdint>
#include <memory>
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