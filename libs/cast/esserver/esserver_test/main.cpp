#include <iostream>
#include "ESServer.h"

int main()
{
    std::cout << "hello from esserver_test" << std::endl;

    hhcast::ESServer server;
    server.StartServer();
    std::cout << "server running: " << server.IsRunning() << std::endl;
    server.StopServer();

    return 0;
}