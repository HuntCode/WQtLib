#pragma once

#include <atomic>

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
    std::atomic<bool> m_running{ false };
    ESServer* m_server = nullptr;
};

} // namespace hhcast