#pragma once

#include "IESServerCallback.h"

#include <atomic>
#include <memory>

namespace hhcast {

class ESServer {
public:
    ESServer();
    ~ESServer();

    int StartServer();
    int StopServer();

    void SetCallback(std::shared_ptr<IESServerCallback> callback);

    bool IsRunning() const;

private:
    std::atomic<bool> m_running{ false };
    std::shared_ptr<IESServerCallback> m_callback = nullptr;
};

} // namespace hhcast