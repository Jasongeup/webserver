#include "websocket_handler.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

WebSocketHandler::WebSocketHandler() : should_stop_(false) {
    worker_thread_ = std::thread(&WebSocketHandler::workerLoop, this);
}

WebSocketHandler::~WebSocketHandler() {
    should_stop_ = true;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool WebSocketHandler::isWebSocketRequest(const std::string& request) {
    return request.find("Upgrade: websocket") != std::string::npos &&
           request.find("Connection: Upgrade") != std::string::npos;
}

std::string WebSocketHandler::generateHandshakeResponse(const std::string& request) {
    // 提取Sec-WebSocket-Key
    std::regex key_regex(R"(Sec-WebSocket-Key:\s*([A-Za-z0-9+/=]+))");
    std::smatch match;
    
    if (!std::regex_search(request, match, key_regex)) {
        return "";
    }
    
    std::string key = match[1].str();
    std::string accept_key = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    // 计算SHA1哈希
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(accept_key.c_str()), accept_key.length(), hash);
    
    // Base64编码
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    
    char* encoded;
    long len = BIO_get_mem_data(bio, &encoded);
    std::string accept = std::string(encoded, len);
    
    BIO_free_all(bio);
    
    std::stringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
    
    return response.str();
}

bool WebSocketHandler::handleConnection(int client_fd, const std::string& request) {
    if (!isWebSocketRequest(request)) {
        return false;
    }
    
    std::string response = generateHandshakeResponse(request);
    if (response.empty()) {
        return false;
    }
    
    // 发送握手响应
    if (send(client_fd, response.c_str(), response.length(), 0) < 0) {
        return false;
    }
    
    // 添加到连接列表
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_[client_fd] = true;
    }
    
    std::cout << "WebSocket connection established for client " << client_fd << std::endl;
    return true;
}

bool WebSocketHandler::sendMessage(int client_fd, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    if (connected_clients_.find(client_fd) == connected_clients_.end()) {
        return false;
    }
    
    // WebSocket帧格式
    std::string frame;
    frame.push_back(0x81); // FIN=1, opcode=1 (text frame)
    
    if (message.length() < 126) {
        frame.push_back(message.length());
    } else if (message.length() < 65536) {
        frame.push_back(126);
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((message.length() >> (i * 8)) & 0xFF);
        }
    }
    
    frame += message;
    
    return send(client_fd, frame.c_str(), frame.length(), 0) > 0;
}

void WebSocketHandler::broadcastMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    for (auto& client : connected_clients_) {
        sendMessage(client.first, message);
    }
}

void WebSocketHandler::closeConnection(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_.erase(client_fd);
    }
    close(client_fd);
    std::cout << "WebSocket connection closed for client " << client_fd << std::endl;
}

void WebSocketHandler::setMessageCallback(std::function<void(const WebSocketMessage&)> callback) {
    message_callback_ = callback;
}

void WebSocketHandler::workerLoop() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !message_queue_.empty() || should_stop_; });
        
        while (!message_queue_.empty()) {
            WebSocketMessage msg = message_queue_.front();
            message_queue_.pop();
            lock.unlock();
            
            if (message_callback_) {
                message_callback_(msg);
            }
            
            lock.lock();
        }
    }
}

std::string WebSocketHandler::base64Encode(const std::string& input) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    
    char* encoded;
    long len = BIO_get_mem_data(bio, &encoded);
    std::string result(encoded, len);
    
    BIO_free_all(bio);
    return result;
}

std::string WebSocketHandler::sha1Hash(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
    return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
}


