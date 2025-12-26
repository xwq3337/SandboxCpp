#include "web_server.h"
#include <iostream>
#include <filesystem>

WebServer::WebServer(int port) : port_(port) {
    server_ = std::make_unique<httplib::Server>();
    
    // 设置路由
    server_->Post("/submit", [this](const httplib::Request& req, httplib::Response& res) {
        handleSubmit(req, res);
    });
    
    server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
    });
    
    // 加载语言配置
    std::string configPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "config" / "languages.json";
    executor_.loadLanguageConfigs(configPath);
}

void WebServer::start() {
    std::cout << "Starting code runner server on port " << port_ << std::endl;
    server_->listen("0.0.0.0", port_);
}

void WebServer::stop() {
    if (server_) {
        server_->stop();
    }
}

void WebServer::handleSubmit(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析 JSON 请求
        json requestJson = json::parse(req.body);
        InputStruct input = requestJson.get<InputStruct>();
        
        std::cout << "Received submission: ID=" << input.submission_id 
                  << ", Language=" << input.language << std::endl;
        
        // 执行代码
        OutputResult output = executor_.execute(input);
        
        // 返回结果
        json responseJson = output;
        res.set_content(responseJson.dump(2), "application/json");
        
        std::cout << "Submission " << input.submission_id 
                  << " completed with verdict: ";
        switch (output.verdict) {
            case Verdict::Accepted: std::cout << "Accepted"; break;
            case Verdict::WrongAnswer: std::cout << "WrongAnswer"; break;
            case Verdict::TimeLimitExceeded: std::cout << "TimeLimitExceeded"; break;
            case Verdict::MemoryLimitExceeded: std::cout << "MemoryLimitExceeded"; break;
            case Verdict::RuntimeError: std::cout << "RuntimeError"; break;
            case Verdict::RestrictedSystemCall: std::cout << "RestrictedSystemCall"; break;
            case Verdict::CompilationError: std::cout << "CompilationError"; break;
            case Verdict::SystemError: std::cout << "SystemError"; break;
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing request: " << e.what() << std::endl;
        
        json errorJson = {
            {"error", e.what()},
            {"status", "SystemError"}
        };
        res.status = 400;
        res.set_content(errorJson.dump(2), "application/json");
    }
}

void WebServer::handleHealth(const httplib::Request& req, httplib::Response& res) {
    json healthJson = {
        {"status", "healthy"},
        {"service", "code_runner"}
    };
    res.set_content(healthJson.dump(2), "application/json");
}
