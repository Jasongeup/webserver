/***********************************************************
 * FileName    : httpRequest.h
 * Description : This header file define a class named httpRequest, 
 *               which parse http-request.
 * 
 * Features    : 
 *   - parse client's http-request form read-buffer, using 
 *     finite status machine
 *   - for post request, check wheather login/register successfully
 *
 * Author      : JasonGe
 * Created on  : 2025/03/27
 * 
***********************************************************/
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../logsys/log.h"
#include "../pool/sqlConnPool.h"
#include "../pool/sqlConnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;  // 哈希表存储头部字段
    std::unordered_map<std::string, std::string> post_;   // 哈希表存储消息体键值对

    static const std::unordered_set<std::string> DEFAULT_HTML;   // 默认的网页
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;   // 识别注册、登录行为
    static int ConverHex(char ch);    // 十六进制转十进制
};


#endif //HTTP_REQUEST_H