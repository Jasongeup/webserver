#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>  // getuid
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<iostream>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>   // splice
#include<sys/uio.h> // writev
#include<sys/sendfile.h>   // sendfile
#include<iostream>
#define BUFFER_SIZE 1024
static const char* status_line[2] = {"200 OK", "500 Internal server error"};
/*CGI服务器
int main(int argc, char* argv[]){
    using namespace std;
    if(argc <= 2){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0)  cout << "errno is" << errno << endl;
    else{
        close(STDOUT_FILENO);
        dup(connfd);
        cout << "abcd" << endl;
        close(connfd);
    }
    close(sock);
    return 0;
}*/
/*Web服务器上的集中写
int main(int argc, char* argv[]){
    using namespace std;
    if(argc <= 3){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number filename" << endl;  // filename是想要获取的服务器端文件名字
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* file_name = argv[3];
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0)  cout << "errno is" << errno << endl;
    else{
        char header_buf[BUFFER_SIZE];   // 用于保存http应答的状态行、头部字段和一个空行的缓存区
        memset(header_buf, '\0', BUFFER_SIZE);   
        char* file_buf;             // 用于存放目标文件内容的应用程序缓存
        struct stat file_stat;      // 用于获取目标文件属性，比如是否为目录，文件大小等
        bool valid = true;          // 记录目标文件是否是有效文件
        int len = 0;                // 缓存区header_buf目前已经使用了多少字节的空间
        if(stat(file_name, &file_stat) < 0) valid = false;   // 目标文件不存在
        else{
            if(S_ISDIR(file_stat.st_mode)){      // 目标文件是一个目录
                valid = false;
            }
            else if(file_stat.st_mode & S_IROTH){  // 当用户有读取目标文件的权限
            //动态分配缓存区file_buf，并指定其大小为目标文件的大小file_stat.st_size加1，然后将目标文件读入缓存区file_buf中
                int fd = open(file_name, O_RDONLY);
                file_buf = new char[file_stat.st_size + 1];
                memset(file_buf, '\0', file_stat.st_size + 1);
                if(read(fd, file_buf, file_stat.st_size) < 0) valid = false;
            }
            else valid = false;       
        }
        if(valid){   // 如果目标文件有效，则发送正常的HTTP应答
        //下面这部分内容将HTTP应答的状态行、“Content-Length”头部字段和一个空行依次加入header_buf中
            ret = snprintf(header_buf, BUFFER_SIZE-1, "%s%s\r\n", "HTTP/1.1", status_line[0]);   //第一行 状态行
            len += ret;
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "Content-length:%d\r\n", file_stat.st_size);   // 输出目标文档的长度
            len += ret;
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "%s", "\r\n");   // 回车，换行
            //利用writev将header_buf和file_buf的内容一并写出
            struct iovec iv[2];
            iv[0].iov_base = header_buf;
            iv[0].iov_len = strlen(header_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(connfd, iv, 2);
        }
        else{  // 如果目标文件无效，则通知客户端服务器发生了“内部错误”
            ret = snprintf(header_buf, BUFFER_SIZE-1, "%s%s\r\n", "HTTP/1.1", status_line[1]);   //第一行 状态行
            len += ret;
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "Content-length:%d\r\n", file_stat.st_size);
            send(connfd, header_buf, strlen(header_buf), 0);
        }
        close(connfd);
        delete [] file_buf;
    }
    close(sock);
    return 0;
}*/
/*用sendfile函数传输文件
int main(int argc, char* argv[]){
    using namespace std;
    if(argc <= 3){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number filename" << endl;  // filename是想要获取的服务器端文件名字
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* file_name = argv[3];
    int filefd = open(file_name, O_RDONLY);
    assert(filefd > 0);
    struct stat stat_buf;
    stat(file_name, &stat_buf);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0)  cout << "errno is" << errno << endl;
    else{
        sendfile(connfd, filefd, NULL, stat_buf.st_size);
        close(connfd);
    }
    close(sock);
    return 0;
}*/
/*用splice函数实现的回射服务器
int main(int argc, char* argv[]){
    using namespace std;
    if(argc <= 2){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number" << endl; 
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0)  cout << "errno is" << errno << endl;
    else{
        int pipefd[2];
        ret = pipe(pipefd);   // 创建管道
        assert(ret != -1);
        ret = splice(connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE); // 将connfd上流入的客户数据定向到管道中
        assert(ret != -1);
        ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
        assert(ret != -1);
        close(connfd);
    }
    close(sock);
    return 0;
}*/
/*同时输出数据到终端和文件的程序
int main(int argc, char* argv[]){
    using namespace std;
    if(argc != 2){
        cout << "usage:" << argv[0] << "<file>" << endl; 
        return 1;
    }
    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
    assert(filefd > 0);
    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    assert(ret != -1);
    int pipefd_file[2];
    ret = pipe(pipefd_file);
    assert(ret != -1);
    ret = splice(STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);  //将标准输入内容输入管道pipefd_stdout
    assert(ret != -1);
    ret = tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK);    //将管道pipefd_stdout的输出复制到管道pipefd_file的输入端
    assert(ret != -1);
    ret = splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);  //将管道pipefd_file的输出定向到文件描述符filefd上，从而将标准输入的内容写入文件
    assert(ret != -1);
    ret = splice(pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);  //将管道pipefd_stdout的输出定向到标准输出，其内容和写入文件的内容完全一致
    assert(ret != -1);
    close(filefd);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    return 0;
}*/
/* 将文件描述符设置为非阻塞的
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}*/
/*测试进程的uid, euid区别
int main(){
    using namespace std;
    uid_t uid = getuid();
    uid_t euid = geteuid();
    cout << "userid is" << uid << ", effective userid is" << euid << endl;
    return 0;
}*/
/*切换用户，将以root身份启动的进程切换为以一个普通用户身份运行
static bool switch_to_user(uid_t user_id, gid_t gp_id){
    if((user_id == 0) && (gp_id == 0)) return false;    // 先确保目标用户不是root
    gid_t gid = getgid();
    uid_t uid = getuid();
    // 确保当前用户是合法用户：root或者目标用户，即只能处理当前用户是这两种用户的情况
    if(((gid != 0) || (uid != 0)) && ((gid != gp_id) || (uid != user_id))) return false;
    if(uid != 0) return true;     // 如果不是root，则已经是目标用户
    // 切换到目标用户
    if((setgid(gp_id) < 0) || (setuid(user_id) < 0)) return false;
    return true;
}*/
//将服务器程序以守护进程的方式进行
bool daemonize(){
    // 创建子进程，关闭父进程，这样可以使程序在后台运行
    pid_t pid = fork();
    if(pid < 0) return false;
    if(pid > 0) exit(0);
    // 设置文件权限掩码。当进程创建新文件（使用open(const char*pathname,int flags,mode_t mode)系统调用）时，文件的权限将是mode＆0777
    umask(0);
    // 创建新的会话，设置本进程为进程组的首领
    pid_t sid = setsid();
    if(sid < 0) return false;
    //切换工作目录
    if((chdir("/")) < 0) return false;
    // 关闭标准输入设备、标准输出设备和标准错误输出设备
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    // 关闭其他已经打开的文件描述符
    // close(fd);
    // 将标准输入、标准输出和标准错误输出都定向到/dev/null文件
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
    return true;
}
