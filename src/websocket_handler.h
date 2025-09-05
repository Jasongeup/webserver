#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

class WebSocketHandler {
public:
    struct WebSocketMessage {
        std::string type;
        std::string data;
        int client_fd;
    };

    WebSocketHandler();
    ~WebSocketHandler();

    // 处理WebSocket连接
    bool handleConnection(int client_fd, const std::string& request);
    
    // 发送消息到客户端
    bool sendMessage(int client_fd, const std::string& message);
    
    // 广播消息到所有连接的客户端
    void broadcastMessage(const std::string& message);
    
    // 关闭连接
    void closeConnection(int client_fd);
    
    // 设置消息处理回调
    void setMessageCallback(std::function<void(const WebSocketMessage&)> callback);
    
    // 检查是否为WebSocket请求
    static bool isWebSocketRequest(const std::string& request);
    
    // 生成WebSocket握手响应
    static std::string generateHandshakeResponse(const std::string& request);

private:
    std::map<int, bool> connected_clients_;
    std::mutex clients_mutex_;
    std::function<void(const WebSocketMessage&)> message_callback_;
    
    // 消息队列和处理线程
    std::queue<WebSocketMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    bool should_stop_;
    
    void workerLoop();
    std::string base64Encode(const std::string& input);
    std::string sha1Hash(const std::string& input);
};


