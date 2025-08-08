#pragma once

#include <string>
#include <map>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    // 构造函数
    HttpRequest() : method("GET"), path("/"), version("HTTP/1.1") {}
};