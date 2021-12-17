#include "http_connection.h"

// http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

// 网站根目录
const char* doc_root = "./root";

int HttpConnection::epollfd_ = -1;
int HttpConnection::user_count_ = 0;
InactiveHandler* HttpConnection::inactive_handler_ = InactiveHandler::instance();

void HttpConnection::init(int sockfd, const sockaddr_in& addr) {
    sockfd_ = sockfd;
    address_ = addr;
    // int reuse = 1;
    // setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef CONN_ET
    addFd(epollfd_, sockfd, true, true);
#else
    addFd(epollfd_, sockfd, true, false);
#endif
    ++user_count_;

    init();
    
    time_t cur_time = time(NULL);
    time_t expire = cur_time + 3*inactive_handler_->time_slot_;
    UtilTimer* timer = new UtilTimer(expire, cbFunc, this);
    inactive_handler_->timers_->addTimer(timer);
    timer_ = timer;
}

void HttpConnection::init() {
    check_state_ = CHECK_STATE_REQUESTLINE;
    linger_ = false;
    method_ = GET;
    url_ = 0;
    version_ = 0;
    content_length_ = 0;
    host_ = 0;
    start_line_ = 0;
    checked_idx_ = 0;
    read_idx_ = 0;
    write_idx_ = 0;
    bytes_have_send_ = 0;
    bytes_to_send_ = 0;

    memset(read_buf_, '\0', READ_BUFFER_SIZE);
    memset(write_buf_, '\0', WRITE_BUFFER_SIZE);
    memset(real_file_, '\0', FILENAME_LEN);
}

void HttpConnection::closeConn(bool real_close) {
    if(real_close && (sockfd_ != -1)) {
        removeFd(epollfd_, sockfd_);
        sockfd_ = -1;
        --user_count_;
        printf("user_cnt: %d\n", user_count_);
        inactive_handler_->timers_->delTimer(timer_);
        printf("close connection\n");
        // printf("delete 1 timer\n");
    }
}

// 由线程池中的工作线程调用，处理HTTP请求的入口
void HttpConnection::process() {
    HTTP_CODE read_ret = processRead();
    if(read_ret == NO_REQUEST) {
        modFd(epollfd_, sockfd_, EPOLLIN);
        return;
    }
    bool write_ret = processWrite(read_ret);
    if(!write_ret) {
        closeConn();
    }
    modFd(epollfd_, sockfd_, EPOLLOUT);
}

bool HttpConnection::read() {
    if(read_idx_ >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
#ifdef CONN_ET
    while(true) {
        bytes_read = recv(sockfd_, read_buf_ + read_idx_, 
                            READ_BUFFER_SIZE - read_idx_, 0);
        if(bytes_read == -1) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            return false;
        } else if(bytes_read == 0) {
            return false;
        }
        read_idx_ += bytes_read;
    }
    return true;
#else
    bytes_read = recv(sockfd_, read_buf_ + read_idx_, 
                        READ_BUFFER_SIZE - read_idx_, 0);
    read_idx_ += bytes_read;
    if(bytes_read <= 0) {
        return false;
    }
    return true;
#endif
}

// 解析客户请求状态机
HttpConnection::HTTP_CODE HttpConnection::processRead() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
            ((line_status = parseLine()) == LINE_OK))
    {
        text = getLine();
        start_line_ = checked_idx_;
        printf("got 1 http line : %s\n", text);

        switch (check_state_)
        {
            case CHECK_STATE_REQUESTLINE: {
                ret = parseRequestLine(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parseHeaders(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) {
                    return doRequest();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parseContent(text);
                if(ret == GET_REQUEST) {
                    return doRequest();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

HttpConnection::LINE_STATUS HttpConnection::parseLine() {
    char tmp;
    for( ; checked_idx_ < read_idx_; ++checked_idx_) {
        tmp = read_buf_[checked_idx_];

        if(tmp == '\r') {
            if(checked_idx_ + 1 == read_idx_) {
                return LINE_OPEN;
            } else if(read_buf_[checked_idx_ + 1] == '\n') {
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if(tmp == '\n') {
            if((checked_idx_ > 1) && (read_buf_[checked_idx_ - 1] == '\r')) {
                read_buf_[checked_idx_ - 1] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpConnection::HTTP_CODE HttpConnection::parseRequestLine(char* text) {
    // 寻找text中第一个出现' ' or '\t位置
    url_ = strpbrk(text, " \t");
    if(!url_) {
        return BAD_REQUEST;
    }
    *url_++ = '\0';

    char* method = text;
    // 不分大小写的字符串匹配
    if(strcasecmp(method, "GET") == 0) {
        method_ = GET;
    } else {
        return BAD_REQUEST;
    }
    //  检索字符串 url_ 中第一个不在字符串 " \t" 中出现的字符下标
    url_ += strspn(url_, " \t");
    version_ = strpbrk(url_, " \t");
    if(!version_) {
        return BAD_REQUEST;
    }
    *version_++ = '\0';
    version_ += strspn(version_, " \t");
    if(strcasecmp(version_, "HTTP/1.1") != 0 && strcasecmp(version_, "HTTP/1.0") != 0) {
        return BAD_REQUEST;
    }

    if(strncasecmp(url_, "http://", 7) == 0) {
        url_ += 7;
        // locate character in string
        url_ = strchr(url_, '/');
    }
    if(!url_ || url_[0] != '/') {
        return BAD_REQUEST;
    }
    
    check_state_ = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::parseHeaders(char* text) {
    if(text[0] == '\0') {
        if(content_length_ != 0) {
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    // 处理Connection头部字段
    } else if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            linger_ = true;
        }
    } else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    } else if(strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    } else {
        printf("unknown header %s\n", text);
    }
    return NO_REQUEST;
}

// 没有真正解析请求体，只是判断是否被完整读入
HttpConnection::HTTP_CODE HttpConnection::parseContent(char* text) {
    if(read_idx_ >= content_length_ + checked_idx_) {
        text[content_length_] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 查找并映射客户请求的文件
HttpConnection::HTTP_CODE HttpConnection::doRequest() {
    strcpy(real_file_, doc_root);
    int len = strlen(doc_root);
    strncpy(real_file_ + len, url_, FILENAME_LEN - len - 1);

    if(stat(real_file_, &file_stat_) < 0) {
        printf("No resource\n");
        return NO_RESOURCE;
    }
    if(!(file_stat_.st_mode & S_IROTH)) { // S_IROTH：文件权限:其他读
        return FORBIDDEN_REQUEST;
    }
    if((S_ISDIR(file_stat_.st_mode))) {
        return BAD_REQUEST;
    }

    int fd = open(real_file_, O_RDONLY);
    file_address_ = (char*)mmap(NULL, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

inline char* HttpConnection::getLine() {
    return read_buf_ + start_line_;
}

inline void HttpConnection::unmap() {
    if(file_address_) {
        munmap(file_address_, file_stat_.st_size);
        file_address_ = 0;
    }
}

// 将服务器请求得到的数据发送给客户端
bool HttpConnection::write() {
    int tmp = 0;
    // int bytes_to_send = write_idx_;
    // no bytes to send
    if(bytes_to_send_ <= 0) {
        modFd(epollfd_, sockfd_, EPOLLIN);
        init();
        return true;
    }
    while(1) {
        tmp = writev(sockfd_, iv_, iv_count_);
        if(tmp <= -1) {
            // TCP写缓冲区没有空间，注册可写事件
            if(errno == EAGAIN) {
                modFd(epollfd_, sockfd_, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send_ -= tmp;
        bytes_have_send_ += tmp;
        
        if(bytes_have_send_ >= iv_[0].iov_len) {
            iv_[0].iov_len = 0;
            iv_[1].iov_base = file_address_ + (bytes_have_send_ - write_idx_);
            iv_[1].iov_len = bytes_to_send_;
        } else {
            iv_[0].iov_base = write_buf_ + bytes_have_send_;
            iv_[0].iov_len = iv_[0].iov_len - bytes_have_send_;
        }

        if(bytes_to_send_ <= 0) {
            unmap();
            if(linger_) {
                init();
                modFd(epollfd_, sockfd_, EPOLLIN);
                return true;
            } else {
                modFd(epollfd_, sockfd_, EPOLLIN);
                return false;
            }
        }
    }
}

void HttpConnection::adjustTimer() {
    if(timer_) {
        time_t cur_time = time(NULL);
        timer_->expire_ = cur_time + 3*inactive_handler_->time_slot_;
        printf("adjust timer once\n");
        inactive_handler_->timers_->adjustTimer(timer_);
    }
}

// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容，处理请求状态机
bool HttpConnection::processWrite(HTTP_CODE ret) {
    switch (ret)
    {
        case INTERNAL_ERROR: {
            addStatusLine(500, error_500_title);
            addHeaders(strlen(error_500_form));
            if(!addContent(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            addStatusLine(400, error_400_title);
            addHeaders(strlen(error_400_form));
            if(!addContent(error_400_form)) {
                return false;
            }
            break; 
        }
        case NO_RESOURCE: {
            addStatusLine(404, error_404_title);
            addHeaders(strlen(error_404_form));
            if(!addContent(error_404_form)) {
                return false;
            }
            break; 
        }
        case FORBIDDEN_REQUEST: {
            addStatusLine(403, error_403_title);
            addHeaders(strlen(error_403_form));
            if(!addContent(error_403_form)) {
                return false;
            }
            break; 
        }
        case FILE_REQUEST: {
            addStatusLine(200, ok_200_title);
            if(file_stat_.st_size != 0) {
                addHeaders(file_stat_.st_size);
                iv_[0].iov_base = write_buf_;
                iv_[0].iov_len = write_idx_;
                iv_[1].iov_base = file_address_;
                iv_[1].iov_len = file_stat_.st_size;
                iv_count_ = 2;
                bytes_to_send_ = write_idx_ + file_stat_.st_size;
                return true;
            } else {
                const char* ok_string = "<html><body></body></html>";
                addHeaders(strlen(ok_string));
                if(!addContent(ok_string)) {
                    return false;
                }
            }
        }
        default:
            return false;
    }
    iv_[0].iov_base = write_buf_;
    iv_[0].iov_len = write_idx_;
    iv_count_ = 1;
    bytes_to_send_ = write_idx_;
    return true;
}

// 往写缓冲区中写入待发送的数据
bool HttpConnection::addResponse(const char* format, ...) {
    if(write_idx_ >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buf_ + write_idx_, 
                WRITE_BUFFER_SIZE - 1 - write_idx_, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - write_idx_)) {
        va_end(arg_list);
        return false;
    }
    write_idx_ += len;
    va_end(arg_list);
    return true;
}

bool HttpConnection::addStatusLine(int status, const char* title) {
    return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConnection::addHeaders(int content_length) {
    return addContentLength(content_length) &&
           addLinger() &&
           addBlankLine();
}

bool HttpConnection::addContentLength(int content_length) {
    return addResponse("Content-Length: %d\r\n", content_length);
}

bool HttpConnection::addLinger() {
    return addResponse("Connection: %s\r\n", (linger_ == true) ? "keep-alive" : "close");
}

bool HttpConnection::addBlankLine() {
    return addResponse("%s", "\r\n");
}

bool HttpConnection::addContent(const char* content) {
    return addResponse("%s", content);
}