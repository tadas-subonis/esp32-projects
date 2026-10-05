#include "tcp.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace net {
namespace {

#ifdef _WIN32
using sock_t = SOCKET;
constexpr sock_t kInvalid = INVALID_SOCKET;

void ensure_wsa()
{
    static bool ready = false;
    if (!ready) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        ready = true;
    }
}

int last_sock_err()
{
    return WSAGetLastError();
}

bool would_block(int err)
{
    return err == WSAEWOULDBLOCK;
}
#else
using sock_t = int;
constexpr sock_t kInvalid = -1;

void ensure_wsa() {}

int last_sock_err()
{
    return errno;
}

bool would_block(int err)
{
    return err == EAGAIN || err == EWOULDBLOCK;
}
#endif

sock_t as_sock(int fd)
{
    return static_cast<sock_t>(fd);
}

int as_fd(sock_t s)
{
    return static_cast<int>(s);
}

int send_flags()
{
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

}  // namespace

int listen_tcp(int port)
{
    ensure_wsa();
    const sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == kInvalid) {
        return -1;
    }
    int yes = 1;
#ifdef _WIN32
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_fd(as_fd(fd));
        return -1;
    }
    if (::listen(fd, 4) < 0) {
        close_fd(as_fd(fd));
        return -1;
    }
    set_nonblock(as_fd(fd));
    return as_fd(fd);
}

int accept_tcp(int listen_fd)
{
    sockaddr_in addr{};
#ifdef _WIN32
    int len = static_cast<int>(sizeof(addr));
#else
    socklen_t len = sizeof(addr);
#endif
    const sock_t fd = ::accept(as_sock(listen_fd), reinterpret_cast<sockaddr*>(&addr), &len);
    if (fd == kInvalid) {
        return -1;
    }
    set_nonblock(as_fd(fd));
    return as_fd(fd);
}

int connect_tcp(const char* host, int port)
{
    ensure_wsa();
    const sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == kInvalid) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close_fd(as_fd(fd));
        return -1;
    }
    set_nonblock(as_fd(fd));
    const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc == 0) {
        return as_fd(fd);
    }
#ifdef _WIN32
    const int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
        close_fd(as_fd(fd));
        return -1;
    }
#else
    if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
        close_fd(as_fd(fd));
        return -1;
    }
#endif
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200 * 1000;  // 200 ms per attempt; callers may retry
    const int sel = ::select(static_cast<int>(fd) + 1, nullptr, &wset, nullptr, &tv);
    if (sel <= 0) {
        close_fd(as_fd(fd));
        return -1;
    }
    int so_error = 0;
#ifdef _WIN32
    int len = sizeof(so_error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) != 0 || so_error != 0) {
#else
    socklen_t len = sizeof(so_error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0 || so_error != 0) {
#endif
        close_fd(as_fd(fd));
        return -1;
    }
    return as_fd(fd);
}

void set_nonblock(int fd)
{
#ifdef _WIN32
    u_long mode = 1;
    ::ioctlsocket(as_sock(fd), FIONBIO, &mode);
#else
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void close_fd(int fd)
{
    if (fd < 0) {
        return;
    }
#ifdef _WIN32
    ::closesocket(as_sock(fd));
#else
    ::close(fd);
#endif
}

int send_all(int fd, const char* data, int len)
{
    int sent = 0;
    int spins = 0;
    while (sent < len) {
#ifdef _WIN32
        const int n = ::send(as_sock(fd), data + sent, len - sent, send_flags());
#else
        const int n = static_cast<int>(::send(fd, data + sent, static_cast<size_t>(len - sent), send_flags()));
#endif
        if (n < 0) {
            if (would_block(last_sock_err())) {
                if (++spins > 2000) {
                    return -1;
                }
#ifdef _WIN32
                ::Sleep(1);
#else
                usleep(1000);
#endif
                continue;
            }
            return -1;
        }
        spins = 0;
        sent += n;
    }
    return sent;
}

int recv_some(int fd, char* data, int cap)
{
#ifdef _WIN32
    const int n = ::recv(as_sock(fd), data, cap, 0);
#else
    const int n = static_cast<int>(::recv(fd, data, static_cast<size_t>(cap), 0));
#endif
    if (n == 0) {
        return -1;  // peer closed
    }
    if (n < 0) {
        if (would_block(last_sock_err())) {
            return 0;
        }
        return -1;
    }
    return n;
}

bool LineBuf::feed(const char* data, int n)
{
    if (n <= 0) {
        return true;
    }
    if (len_ + n >= static_cast<int>(sizeof(buf_))) {
        len_ = 0;
        return false;
    }
    std::memcpy(buf_ + len_, data, static_cast<size_t>(n));
    len_ += n;
    return true;
}

bool LineBuf::pop_line(char* out, size_t cap)
{
    int i = 0;
    while (i < len_ && buf_[i] != '\n') {
        ++i;
    }
    if (i >= len_) {
        return false;
    }
    int line_len = i;
    if (line_len > 0 && buf_[line_len - 1] == '\r') {
        --line_len;
    }
    if (static_cast<size_t>(line_len) + 1 > cap) {
        line_len = static_cast<int>(cap) - 1;
    }
    std::memcpy(out, buf_, static_cast<size_t>(line_len));
    out[line_len] = 0;
    const int rest = len_ - (i + 1);
    if (rest > 0) {
        std::memmove(buf_, buf_ + i + 1, static_cast<size_t>(rest));
    }
    len_ = rest;
    return true;
}

}  // namespace net
