/***********************************************
 * FileName    : httpRequest.cpp
 * Description : see httpRequest.h
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/27
************************************************/
#include "httpRequest.h"
using namespace std;

/* 默认html文件 */
const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

/* 返回客户是否要求保持TCP长连接 */
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) { // 逐行读取http请求
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2); // 搜索换行符获取行结尾
        std::string line(buff.Peek(), lineEnd);  // 用指针（迭代器）来初始化string?
        switch(state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;    
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; } // ?
        buff.RetrieveUntil(lineEnd + 2);  // ?
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

/* 获取http请求的路径 */
void HttpRequest::ParsePath_() {
    if(path_ == "/") {   // 默认是index.html
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

/* 解析请求行, 利用正则表达式 */
bool HttpRequest::ParseRequestLine_(const string& line) {  
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];  // 注意这里path只包含资源路径，即“/index”这种，与书里的不太一样
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;  
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;   // 虽然这里是finish，但只要buff中还有没读完的，parse还会继续循环读
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

/* 将十六进制字符转换成数字（用于在后续ParseFromUrlencoded_中转换为ASCII）*/
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

/* 分析post请求 */
void HttpRequest::ParsePost_() {
    // 如果是提交了html表单（以键值对的形式）
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();    // 读取消息体中的键值对，并存储在post_中
        if(DEFAULT_HTML_TAG.count(path_)) {  // 是不是注册或者登录请求
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1); // true为登录，反之是注册
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {    // 登录/注册失败
                    path_ = "/error.html";
                }
            }
        }
    }   
}

/* 提取消息体中的键值对，存储在post_中 */
void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);  // 遇到等号，说明key遍历完毕
            j = i + 1;
            break;
        case '+':   // +代表空格
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]); // 十六进制转十进制数字
            body_[i + 2] = num % 10 + '0';  // 转换成ASCII码
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':    // 键值对之间以&分割
            value = body_.substr(j, i - j);  // 遇到&,表示一个键值对遍历完毕
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {  // 最后一个键值对的末尾是没有&，所以要额外处理
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

/* 验证用户登录行为 */
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { // sql查询语句
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);    // 保存查询结果？
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {   // 表示获取一行
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);  // 应该是用户名，密码
        string password(row[1]);
        if(isLogin) {   // 如果是登录，要求密码正确
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else {      // 如果是注册，要求用户名未被使用，而这里成功查询到该用户名，故返回用户名已使用
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        // 往数据库添加数据
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) {   // 查询是否添加成功
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

/* 返回用户请求的文件路径 */
std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}