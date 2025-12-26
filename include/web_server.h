#pragma once

#include "code_executor.h"
#include <httplib.h>
#include <memory>

class WebServer {
public:
    WebServer(int port);
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
private:
    int port_;
    std::unique_ptr<httplib::Server> server_;
    CodeExecutor executor_;
    
    // 处理提交请求
    void handleSubmit(const httplib::Request& req, httplib::Response& res);
    
    // 处理健康检查
    void handleHealth(const httplib::Request& req, httplib::Response& res);
};
