#include "chat_handler.h"
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

std::string extract_json_value(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":\"";
    size_t start = json.find(pattern);
    if (start == std::string::npos) return "";
    
    start += pattern.length();
    size_t end = json.find('"', start);
    if (end == std::string::npos) return "";
    
    return json.substr(start, end - start);
}

std::string send_http_request(const std::string& host, int port, const std::string& path, const std::string& body) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return "";
    }

    std::string request = "POST " + path + " HTTP/1.1\r\n"
                         "Host: " + host + ":" + std::to_string(port) + "\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n"
                         + body;

    send(sock, request.c_str(), request.size(), 0);

    char buffer[4096];
    std::string response;
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes_received);
    }

    close(sock);
    return response;
}

void chat_handler(const HttpRequest& req, HttpResponse& res) {
    // 1. 解析前端请求
    std::string message;
    size_t start = req.body.find("\"message\":\"");
    if (start != std::string::npos) {
        start += 10; // 跳过 "message":"
        size_t end = req.body.find('"', start);
        if (end != std::string::npos) {
            message = req.body.substr(start, end - start);
        }
    }
    
    if (message.empty()) {
        res.set_status_code(400);
        res.set_content("{\"error\": \"Invalid request format\"}", "application/json");
        return;
    }

    // 2. 构造请求体
    std::string req_body = "{\"message\": \"" + message + "\"}";

    // 3. 发送HTTP请求
    std::string api_response = send_http_request("127.0.0.1", 8000, "/chat", req_body);
    
    // 4. 处理响应
    if (!api_response.empty()) {
        // 提取JSON响应体
        std::string response_text = extract_json_value(api_response, "response");
        if (!response_text.empty()) {
            std::string json_response = "{\"response\": \"" + response_text + "\"}";
            res.set_status_code(200);
            res.set_content(json_response, "application/json");
            return;
        }
    }
    
    // 错误处理
    res.set_status_code(500);
    res.set_content("{\"error\": \"LLM service unavailable\"}", "application/json");
}