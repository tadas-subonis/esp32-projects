#pragma once

#include <cstdint>

namespace net {

struct UdpAddr {
    uint32_t host = 0;  // network byte order
    uint16_t port = 0;  // host byte order
};

int listen_udp(int port);
/** Connected UDP socket (send/recv without addr). */
int connect_udp(const char* host, int port);

int udp_send(int fd, const void* data, int len);
int udp_send_to(int fd, const void* data, int len, const UdpAddr& addr);
/** Returns bytes, 0 if would-block, -1 on error. */
int udp_recv(int fd, void* data, int cap, UdpAddr* from);

bool udp_addr_equal(const UdpAddr& a, const UdpAddr& b);
void udp_addr_clear(UdpAddr* a);
bool udp_addr_valid(const UdpAddr& a);

}  // namespace net
