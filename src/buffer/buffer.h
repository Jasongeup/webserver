/***********************************************************
 * FileName    : buffer.h
 * Description : This header file define a class named buffer, 
 *               which as the buffer for httpConn to read/write
 *               data in memory.
 * 
 * Features    : 
 *   - be both write buffer and read buffer
 *   - resize space automatically
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/28
 * 
***********************************************************/


 #ifndef BUFFER_H
 #define BUFFER_H
 #include <cstring>   //perror
 #include <iostream>
 #include <unistd.h>  // write
 #include <sys/uio.h> //readv
 #include <vector>
 #include <atomic>
 #include <assert.h>
 class Buffer {
 public:
     Buffer(int initBuffSize = 1024);
     ~Buffer() = default;
 
     size_t WritableBytes() const;       
     size_t ReadableBytes() const ;
     size_t PrependableBytes() const;
 
     const char* Peek() const;
     void EnsureWriteable(size_t len);
     void HasWritten(size_t len);
 
     void Retrieve(size_t len);
     void RetrieveUntil(const char* end);
 
     void RetrieveAll() ;
     std::string RetrieveAllToStr();
 
     const char* BeginWriteConst() const;
     char* BeginWrite();
 
     void Append(const std::string& str);
     void Append(const char* str, size_t len);
     void Append(const void* data, size_t len);
     void Append(const Buffer& buff);
 
     ssize_t ReadFd(int fd, int* Errno);
     ssize_t WriteFd(int fd, int* Errno);
 
 private:
     char* BeginPtr_();
     const char* BeginPtr_() const;
     void MakeSpace_(size_t len);
    
     // buffer的空间大致可分为[beginptr, readpos, writepos, buffer.end()], readpos-writepos是可读的空间
     // 而其他空间都是空闲的，当然如果没有整理buffer情况下，可写的只有writepos-buffer.end()
     std::vector<char> buffer_; 
     std::atomic<std::size_t> readPos_;  // 表示读缓冲区的起始位置的偏移量
     std::atomic<std::size_t> writePos_;  // 表示写缓冲区的起始位置的偏移量
 };
 
 #endif //BUFFER_H