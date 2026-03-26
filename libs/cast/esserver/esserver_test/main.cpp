#include <iostream>
#include <memory>
#include <string>

#include "ESServer.h"

class TestCallback : public hhcast::IESServerCallback {
public:
    void OnConnect(uint32_t streamId, const std::string& name, const std::string& ip) override
    {
        std::cout << "[TestCallback] OnConnect streamId=" << streamId
                  << ", name=" << name
                  << ", ip=" << ip << std::endl;
    }

    void OnDisconnect(uint32_t streamId) override
    {
        std::cout << "[TestCallback] OnDisconnect streamId=" << streamId << std::endl;
    }

    void OnVideoData(uint32_t streamId, const uint8_t* data, size_t size) override
    {
        std::cout << "[TestCallback] OnVideoData streamId=" << streamId
                  << ", size=" << size << std::endl;
    }

    void OnAudioData(uint32_t streamId, const uint8_t* data, size_t size) override
    {
        std::cout << "[TestCallback] OnAudioData streamId=" << streamId
                  << ", size=" << size << std::endl;
    }
};

int main()
{
    std::cout << "hello from esserver_test" << std::endl;

    hhcast::ESServer server;
    auto callback = std::make_shared<TestCallback>();
    server.SetCallback(callback);

    int ret = server.StartServer();
    std::cout << "StartServer ret = " << ret << std::endl;
    std::cout << "server running: " << server.IsRunning() << std::endl;
    std::cout << "input q to stop server" << std::endl;

    std::string cmd;
    while (std::getline(std::cin, cmd)) {
        if (cmd == "q" || cmd == "Q") {
            break;
        }
    }

    server.StopServer();
    return 0;
}