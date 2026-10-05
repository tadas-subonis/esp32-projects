#include "udp.hpp"

#include "tcp.hpp"

#include <cerrno>
#include <cstring>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
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

}  // namespace

bool udp_addr_equal(const UdpAddr& a, const UdpAddr& b)
{
    return a.host == b.host && a.port == b.port;
}

void udp_addr_clear(UdpAddr* a)
{
    if (a) {
        *a = UdpAddr{};
    }
}

bool udp_addr_valid(const UdpAddr& a)
{
    return a.host != 0 && a.port != 0;
}

int listen_udp(int port)
{
    ensure_wsa();
    const sock_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
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
    set_nonblock(as_fd(fd));
    return as_fd(fd);
}

int connect_udp(const char* host, int port)
{
    ensure_wsa();
    const sock_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
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
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_fd(as_fd(fd));
        return -1;
    }
    set_nonblock(as_fd(fd));
    return as_fd(fd);
}

int udp_send(int fd, const void* data, int len)
{
    if (fd < 0 || data == nullptr || len <= 0) {
        return -1;
    }
#ifdef _WIN32
    const int n = ::send(as_sock(fd), static_cast<const char*>(data), len, 0);
#else
    const int n = static_cast<int>(::send(fd, data, static_cast<size_t>(len), 0));
#endif
    if (n < 0) {
        if (would_block(last_sock_err())) {
            return 0;
        }
        return -1;
    }
    return n;
}

int udp_send_to(int fd, const void* data, int len, const UdpAddr& addr)
{
    if (fd < 0 || data == nullptr || len <= 0) {
        return -1;
    }
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = addr.host;
    sa.sin_port = htons(addr.port);
#ifdef _WIN32
    const int n = ::sendto(as_sock(fd), static_cast<const char*>(data), len, 0,
                           reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
#else
    const int n = static_cast<int>(::sendto(fd, data, static_cast<size_t>(len), 0,
                                            reinterpret_cast<sockaddr*>(&sa), sizeof(sa)));
#endif
    if (n < 0) {
        if (would_block(last_sock_err())) {
            return 0;
        }
        return -1;
    }
    return n;
}

int udp_recv(int fd, void* data, int cap, UdpAddr* from)
{
    if (fd < 0 || data == nullptr || cap <= 0) {
        return -1;
    }
    sockaddr_in sa{};
#ifdef _WIN32
    int salen = sizeof(sa);
    const int n = ::recvfrom(as_sock(fd), static_cast<char*>(data), cap, 0,
                             reinterpret_cast<sockaddr*>(&sa), &salen);
#else
    socklen_t salen = sizeof(sa);
    const int n = static_cast<int>(::recvfrom(fd, data, static_cast<size_t>(cap), 0,
                                              reinterpret_cast<sockaddr*>(&sa), &salen));
#endif
    if (n < 0) {
        if (would_block(last_sock_err())) {
            return 0;
        }
        return -1;
    }
    if (from) {
        from->host = sa.sin_addr.s_addr;
        from->port = ntohs(sa.sin_port);
    }
    return n;
}

}  // namespace net
