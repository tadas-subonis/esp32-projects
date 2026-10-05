#pragma once

#include <cstddef>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace net {

int listen_tcp(int port);
int accept_tcp(int listen_fd);
int connect_tcp(const char* host, int port);
void set_nonblock(int fd);
void close_fd(int fd);
int send_all(int fd, const char* data, int len);
int recv_some(int fd, char* data, int cap);

class LineBuf {
public:
    bool feed(const char* data, int n);
    bool pop_line(char* out, size_t cap);

private:
    char buf_[8192]{};
    int len_ = 0;
};

}  // namespace net
