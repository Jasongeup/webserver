#include "FileHandler.h"
#include "../httpConnection/httpconn.h"
#include "../file/FileService.h"
#include <json/json.h>

void FileHandler::handleUpload(HttpConn* conn) {
    // 检查用户是否登录
    if (!conn->getUser().isValid()) {
        conn->setResponse(HttpResponse(401, "Unauthorized"));
        return;
    }
    
    // 解析请求体
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(conn->getRequestBody(), root)) {
        conn->setResponse(HttpResponse(400, "Invalid JSON"));
        return;
    }
    
    std::string filename = root["filename"].asString();
    std::string content = root["content"].asString();
    
    // 调用文件服务
    if (FileService::getInstance().uploadFile(conn->getUser(), filename, content)) {
        conn->setResponse(HttpResponse(200, "File uploaded successfully"));
    } else {
        conn->setResponse(HttpResponse(500, "File upload failed"));
    }
}

void FileHandler::handleDownload(HttpConn* conn) {
    // 检查用户是否登录
    if (!conn->getUser().isValid()) {
        conn->setResponse(HttpResponse(401, "Unauthorized"));
        return;
    }
    
    // 从查询参数获取文件ID
    std::string fileId = conn->getRequest().getParam("fileId");
    if (fileId.empty()) {
        conn->setResponse(HttpResponse(400, "Missing fileId parameter"));
        return;
    }
    
    // 调用文件服务
    std::string content = FileService::getInstance().downloadFile(conn->getUser(), fileId);
    if (content.empty()) {
        conn->setResponse(HttpResponse(404, "File not found"));
    } else {
        HttpResponse response(200, content);
        response.addHeader("Content-Type", "application/octet-stream");
        conn->setResponse(response);
    }
}

void FileHandler::handleList(HttpConn* conn) {
    // 检查用户是否登录
    if (!conn->getUser().isValid()) {
        conn->setResponse(HttpResponse(401, "Unauthorized"));
        return;
    }
    
    // 获取用户文件列表
    auto files = FileService::getInstance().getUserFiles(conn->getUser());
    
    // 构建JSON响应
    Json::Value root;
    Json::Value fileList(Json::arrayValue);
    
    for (const auto& file : files) {
        Json::Value fileInfo;
        fileInfo["id"] = file.id;
        fileInfo["filename"] = file.filename;
        fileInfo["size"] = static_cast<Json::UInt>(file.size);
        fileInfo["uploadTime"] = file.uploadTime;
        fileList.append(fileInfo);
    }
    
    root["files"] = fileList;
    
    Json::StreamWriterBuilder builder;
    conn->setResponse(HttpResponse(200, Json::writeString(builder, root)));
}