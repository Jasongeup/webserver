/***********************************************
 * FileName    : buffer.cpp
 * Description : see buffer.h
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/28
************************************************/
#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

/* 获取缓冲区中可读数据的长度 */
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

/* 返回缓冲区中可写空间的长度 */
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

/* 返回缓冲区中在当前读取位置之前的空闲区域大小，即缓冲区开头到读指针之间的大小 */
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

/* 返回一个指向当前可读数据起始位置的指针 */
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

/* 表示从缓冲区中取走了已读取的数据，即让读起始位置向后偏移len */
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

/* 表示从缓冲区中取走了peek()到end()之间的数据 */
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

/* 表示清空缓冲区的数据，重置读取和写入位置 */
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

/* 将缓冲区中所有可读数据转换为一个string,然后清空缓冲区 */
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

/* 返回缓冲区中可写空间的起始const指针 */
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

/* 返回缓冲区中可写空间的起始指针 */
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

/* 表示写入了len个字节，即让写起始位置向后移动len*/
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

/* 往缓冲区中写入数据，有多个重载方法，要更新可写区域的起始位置 */
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

/* 确保缓冲区空间大于len, 否则就扩容 */
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

/* 将文件内容分散读到缓冲区中，超出自动扩容 */
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_; // 先填到缓冲区中
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;    // 如果缓冲区中不够，会填到这个局部buff中
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {  // 如果缓冲区足够填下，则直接更新写起始位置
        writePos_ += len;
    }
    else {// 如果缓冲区填不下，则先更新写起始位置到缓冲区末尾，再用append插入剩下的数据（会扩容），由append更新新的写起始
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

/* 将缓冲区全部可读内容写到文件中，更新读起始位置 */
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

/* 返回指向缓冲区起始位置的指针 */
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

/* 返回指向缓冲区起始位置的const指针 */
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

/* 若缓冲区有len大小的空闲，则整理空闲空间，否则扩容 */
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) { // 如果缓冲区所有剩余空间都不足len，则调整缓冲区大小
        buffer_.resize(writePos_ + len + 1);
    } 
    else {  // 如果空间够，则整理空间，把缓冲区数据移到开头，更新读、写起始位置
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}