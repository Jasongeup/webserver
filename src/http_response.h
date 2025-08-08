#pragma once

#include <string>
#include <map>

class HttpResponse {
public:
    int status_code;
    std::map<std::string, std::string> headers;
    std::string content;

    // 构造函数
    HttpResponse() : status_code(200) {}
    
    // 设置状态码
    void set_status_code(int code) {
        status_code = code;
    }
    
    // 设置内容
    void set_content(const std::string& data, const std::string& content_type) {
        content = data;
        headers["Content-Type"] = content_type;
        headers["Content-Length"] = std::to_string(data.size());
    }
};