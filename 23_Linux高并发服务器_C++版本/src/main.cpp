#include "ConfigMgr.h"
#include "server.h"
#include <csignal>

// 全局（翻译单元）指针，用于 signal handler 中访问
static Server* g_server = nullptr;

// 信号处理函数
void signal_handler(int sig){
    if(g_server){
        std::cout << "\nReceived signal " << sig << ", stopping server...\n";
        g_server->stop();
    }
}

int main(){
    ConfigMgr& cfg = ConfigMgr::Inst();
    if(!cfg.loadFromFile("config.ini")){
        std::cerr << "Failed to load config.ini\n";
        return 1;
    }

    int port = cfg.get<int>("server.port", 12345);
    std::cout << "server.port = " << port << std::endl;

    Server server(port);

    g_server = &server;

    // 注册信号处理器
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.run();

    return 0;
}

