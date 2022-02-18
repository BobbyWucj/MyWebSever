#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include "locker.h"
#include "tools_func.h"
#include "lst_timer.h"
#include "inactive_handler.h"
#include "config.h"

class UtilTimer;
class InactiveHandler;

class HttpConnection
{
public:
    HttpConnection() {};
    ~HttpConnection() {}

public:
    // 文件名最大长度
    static const int FILENAME_LEN = 200;
    // 读缓冲大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // HTTP请求方法，暂时只支持GET
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };
    // 主状态机状态
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 请求结果
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 行的读取状态
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    // 初始化新连接
    void init(int sockfd, const sockaddr_in& addr);
    // 关闭连接
    void closeConn(bool real_close = true);
    // 处理客户请求
    void process();
    // 非阻塞读
    bool read();
    // 非阻塞写
    bool write();

    void adjustTimer();

private:
    void init();
    // 解析HTTP请求
    HTTP_CODE processRead();
    // 填充HTTP应答
    bool processWrite(HTTP_CODE ret);

    // 这组函数将被processRead调用以分析HTTP请求
    HTTP_CODE parseRequestLine(char* text);
    HTTP_CODE parseHeaders(char* text);
    HTTP_CODE parseContent(char* text);
    HTTP_CODE doRequest();
    char* getLine();
    LINE_STATUS parseLine();

    // 这组函数将被processWrite调用以填充HTTP应答
    void unmap();
    bool addResponse(const char* format, ...);
    bool addContent(const char* content);
    bool addStatusLine(int status, const char* title);
    bool addHeaders(int content_length);
    bool addContentLength(int content_length);
    bool addLinger();
    bool addBlankLine();

public:
    static int epollfd_;
    static int user_count_;
    static InactiveHandler* inactive_handler_;

    // 此HTTP连接的socket
    int sockfd_;
    // 客户的socket地址
    sockaddr_in address_;
    // 定时器
    UtilTimer* timer_;

private:
    // 读缓冲区
    char read_buf_[READ_BUFFER_SIZE];
    // 下一个需要读入的位置
    int read_idx_;
    // 正在分析的字符在读缓冲区中的位置
    int checked_idx_;
    // 正在解析的行的起始位置
    int start_line_;
    // 写缓冲区
    char write_buf_[WRITE_BUFFER_SIZE];
    // 下一个需要写入的位置，也是写缓冲区中待发送的字节数
    int write_idx_;
    int bytes_to_send_;
    int bytes_have_send_;
    // 主状态机当前状态
    CHECK_STATE check_state_;
    // 请求方法
    METHOD method_;

    // 客户请求的目标文件的完整路径，doc_root_ + url_, doc_root_为网站根目录
    char real_file_[FILENAME_LEN];
    // 客户请求的目标文件名
    char* url_;
    // HTTP协议版本号
    char* version_;
    // 主机名
    char* host_;
    // HTTP请求的消息体长度
    int content_length_;
    // 是否要保持连接
    bool linger_;

    // 客户请的的目标文件被mmap到内存中的地址
    char* file_address_;
    // 目标文件的信息
    struct stat file_stat_;
    // 采用writev来执行集中写操作，定义内存块数据结构
    struct iovec iv_[2];
    int iv_count_;
};

extern void cbFunc(HttpConnection* user_data);

#endif // HTTP_CONN_H
