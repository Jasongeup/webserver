#include "FileService.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

FileService& FileService::getInstance() {
    static FileService instance;
    return instance;
}

FileService::FileService() {
    // 确保上传目录存在
    fs::create_directories(getStoragePath());
}

bool FileService::uploadFile(const User& user, const std::string& filename, const std::string& content) {
    std::string fileId = generateFileId();
    std::string userDir = getStoragePath() + "/" + user.getId();
    fs::create_directories(userDir);
    
    std::ofstream file(userDir + "/" + fileId);
    if (!file.is_open()) return false;
    
    file << content;
    file.close();
    
    // 记录文件元数据
    std::ofstream meta(userDir + "/.meta");
    meta << fileId << "\t" << filename << "\t" << content.size() << "\t" << std::time(nullptr) << "\n";
    meta.close();
    
    return true;
}

std::string FileService::downloadFile(const User& user, const std::string& fileId) {
    std::string filePath = getStoragePath() + "/" + user.getId() + "/" + fileId;
    std::ifstream file(filePath);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<FileInfo> FileService::getUserFiles(const User& user) {
    std::vector<FileInfo> files;
    std::string metaPath = getStoragePath() + "/" + user.getId() + "/.meta";
    std::ifstream meta(metaPath);
    
    if (!meta.is_open()) return files;
    
    std::string line;
    while (std::getline(meta, line)) {
        std::istringstream iss(line);
        FileInfo info;
        if (iss >> info.id >> info.filename >> info.size >> info.uploadTime) {
            files.push_back(info);
        }
    }
    
    return files;
}

std::string FileService::generateFileId() const {
    // 生成唯一文件ID（实际项目中应使用更健壮的方法）
    return std::to_string(std::time(nullptr));
}

std::string FileService::getStoragePath() const {
    return "uploads";
}