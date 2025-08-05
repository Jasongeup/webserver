#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "../httpConnection/httpconn.h"

class FileHandler {
public:
    static void handleUpload(HttpConn* conn);
    static void handleDownload(HttpConn* conn);
    static void handleList(HttpConn* conn);
};

#endif // FILE_HANDLER_H