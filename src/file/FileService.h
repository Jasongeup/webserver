#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <string>
#include <vector>
#include "../user/User.h"

struct FileInfo {
    std::string id;
    std::string filename;
    std::string uploadTime;
    size_t size;
};

class FileService {
public:
    static FileService& getInstance();
    
    // 上传文件
    bool uploadFile(const User& user, const std::string& filename, const std::string& content);
    
    // 下载文件
    std::string downloadFile(const User& user, const std::string& fileId);
    
    // 获取用户文件列表
    std::vector<FileInfo> getUserFiles(const User& user);
    
private:
    FileService();
    std::string generateFileId() const;
    std::string getStoragePath() const;
};

#endif // FILE_SERVICE_H