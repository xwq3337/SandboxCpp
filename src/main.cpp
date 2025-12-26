#include "web_server.h"
#include <iostream>
#include <csignal>

WebServer* g_server = nullptr;

void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    std::cout << "Code Runner Service" << std::endl;
    std::cout << "===================" << std::endl;
    
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    WebServer server(port);
    g_server = &server;
    
    server.start();
    
    return 0;
}
