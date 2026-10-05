# `smoltcp` (0.12.0)

## What it does in this repo

`smoltcp` is a lightweight TCP/IP stack often used in embedded environments. This repo includes it (with `default-features = false`) and enables multiple protocol/socket features.

It’s also enabled as a feature for `esp-radio` (`smoltcp`) so the Wi‑Fi stack can integrate with it.

## Key links

- API docs (docs.rs): https://docs.rs/smoltcp/0.12.0/smoltcp/
- Crate page: https://crates.io/crates/smoltcp

## How the API is organized (important!)

The crate docs describe `smoltcp` as layered:

- `smoltcp::socket`: TCP/UDP/ICMP/raw socket state machines + buffering
- `smoltcp::iface`: interface logic (routing packets between device + sockets)
- `smoltcp::phy`: “device” abstraction (your NIC/Wi‑Fi driver)
- `smoltcp::wire`: packet parsing/encoding + IP address types
- `smoltcp::time`: time types used by the stack

Crate root: https://docs.rs/smoltcp/0.12.0/smoltcp/

## Practical note

`smoltcp` configuration tends to be buffer-heavy—plan allocations carefully (see `docs/memory-alloc.md`).

## Typical usage pattern (embedded)

At a high level, you:

1) **Provide a device** implementing `smoltcp::phy` traits (usually done by your Wi‑Fi/Ethernet driver)
2) **Create an interface** (`smoltcp::iface`) with addresses/routes
3) **Create sockets** (`smoltcp::socket`), with explicit RX/TX buffers
4) **Poll** the interface regularly, passing time (`smoltcp::time`)

In many embedded apps you won’t interact with `smoltcp` directly because `embassy-net` wraps it with a more opinionated API (see `docs/crates/embassy-net.md`).

## Code sketch: TCP socket buffers + polling loop

This is a *shape* of smoltcp code. The exact device + interface construction depends on your hardware driver.

```rust
// PSEUDO-CODE / STRUCTURE EXAMPLE (device construction omitted)

use smoltcp::iface::{Config as IfaceConfig, Interface, SocketSet};
use smoltcp::socket::tcp::{Socket as TcpSocket, SocketBuffer};
use smoltcp::time::Instant;
use smoltcp::wire::{IpAddress, IpCidr};

// 1) Build a device (phy::Device) for your hardware.
// let mut device = ...;

// 2) Create interface with IP config.
// let mut iface = Interface::new(IfaceConfig::new(...), &mut device, Instant::from_millis(0));
// iface.update_ip_addrs(|addrs| addrs.push(IpCidr::new(IpAddress::v4(192,168,1,123), 24)).unwrap());

// 3) Create sockets with explicit buffers.
let mut rx_storage = [0u8; 1024];
let mut tx_storage = [0u8; 1024];
let tcp_rx = SocketBuffer::new(&mut rx_storage);
let tcp_tx = SocketBuffer::new(&mut tx_storage);
let tcp_socket = TcpSocket::new(tcp_rx, tcp_tx);

let mut sockets_storage = [None; 1];
let mut sockets = SocketSet::new(&mut sockets_storage[..]);
let tcp_handle = sockets.add(tcp_socket);

// 4) Poll loop (usually driven by a timer/interrupt + RX notifications).
loop {
    let now = Instant::from_millis(0); // replace with real time source
    // iface.poll(now, &mut device, &mut sockets);

    // let mut socket = sockets.get_mut::<TcpSocket>(tcp_handle);
    // if !socket.is_open() { socket.connect((remote_ip, remote_port), local_port).unwrap(); }
    // if socket.can_send() { socket.send_slice(b"hello").unwrap(); }
    // if socket.can_recv() { let _n = socket.recv(|buf| (buf.len(), ())).unwrap(); }
}
```

## Complete API inventory

Full API index: See local docs at `target/riscv32imc-unknown-none-elf/doc/smoltcp/all.html`

**Note**: `smoltcp` is a large crate (68 structs, 55 enums, 7 traits, 25+ constants, 9 type aliases). This is the complete list organized by module.

### Module: `smoltcp::iface` (Network Interface)

#### Structs
- **`iface::Config`**: Interface configuration.
- **`iface::Context`**: Interface context for polling operations.
- **`iface::Interface`**: The central interface object you poll (most important type).
- **`iface::Route`**: Routing table entry.
- **`iface::RouteTableFull`**: Error when route table is full.
- **`iface::Routes`**: Collection of routes.
- **`iface::SocketHandle`**: Handle to a socket in a `SocketSet`.
- **`iface::SocketSet`**: Collection storing sockets (no heap required if you provide storage).
- **`iface::SocketStorage`**: Storage for socket handles.

#### Enums
- **`iface::MulticastError`**: Error for multicast operations.
- **`iface::PollIngressSingleResult`**: Result of polling a single ingress packet.
- **`iface::PollResult`**: Result of interface polling operation.

### Module: `smoltcp::phy` (Physical Layer / Device Abstraction)

#### Structs
- **`phy::ChecksumCapabilities`**: Device checksum offload capabilities.
- **`phy::DeviceCapabilities`**: Device capabilities (MTU, etc.).
- **`phy::FaultInjector`**: Fault injection for testing.
- **`phy::PacketMeta`**: Packet metadata from device.
- **`phy::PcapWriter`**: PCAP file writer for packet capture.
- **`phy::Tracer`**: Packet tracer for debugging.

#### Enums
- **`phy::Checksum`**: Checksum configuration.
- **`phy::Medium`**: Network medium type (Ethernet, etc.).
- **`phy::PcapLinkType`**: PCAP link type.
- **`phy::PcapMode`**: PCAP capture mode.

#### Traits
- **`phy::Device`**: Trait for network devices (implemented by your Wi‑Fi/Ethernet driver).
- **`phy::PcapSink`**: Trait for PCAP output.
- **`phy::RxToken`**: Token for receiving a packet.
- **`phy::TxToken`**: Token for transmitting a packet.

### Module: `smoltcp::socket` (Socket Implementations)

#### Enums
- **`socket::Socket`**: Enum of all socket types.

#### Module: `smoltcp::socket::tcp` (TCP Sockets)

#### Structs
- **`socket::tcp::Socket`**: TCP socket state machine.

#### Enums
- **`socket::tcp::CongestionControl`**: TCP congestion control algorithm.
- **`socket::tcp::ConnectError`**: Error connecting TCP socket.
- **`socket::tcp::ListenError`**: Error listening on TCP socket.
- **`socket::tcp::RecvError`**: Error receiving TCP data.
- **`socket::tcp::SendError`**: Error sending TCP data.
- **`socket::tcp::State`**: TCP socket state (Closed, Listen, SynSent, etc.).

#### Type Aliases
- **`socket::tcp::SocketBuffer`**: Type alias for TCP socket buffer.

#### Module: `smoltcp::socket::udp` (UDP Sockets)

#### Structs
- **`socket::udp::Socket`**: UDP socket.
- **`socket::udp::UdpMetadata`**: Metadata for UDP packets.

#### Enums
- **`socket::udp::BindError`**: Error binding UDP socket.
- **`socket::udp::RecvError`**: Error receiving UDP.
- **`socket::udp::SendError`**: Error sending UDP.

#### Type Aliases
- **`socket::udp::PacketBuffer`**: Type alias for UDP packet buffer.
- **`socket::udp::PacketMetadata`**: Type alias for UDP packet metadata.

#### Module: `smoltcp::socket::icmp` (ICMP Sockets)

#### Structs
- **`socket::icmp::Socket`**: ICMP socket.

#### Enums
- **`socket::icmp::BindError`**: Error binding ICMP socket.
- **`socket::icmp::Endpoint`**: ICMP endpoint.
- **`socket::icmp::RecvError`**: Error receiving ICMP.
- **`socket::icmp::SendError`**: Error sending ICMP.

#### Type Aliases
- **`socket::icmp::PacketBuffer`**: Type alias for ICMP packet buffer.
- **`socket::icmp::PacketMetadata`**: Type alias for ICMP packet metadata.

#### Module: `smoltcp::socket::raw` (Raw IP Sockets)

#### Structs
- **`socket::raw::Socket`**: Raw IP socket.

#### Enums
- **`socket::raw::BindError`**: Error binding raw socket.
- **`socket::raw::RecvError`**: Error receiving raw packets.
- **`socket::raw::SendError`**: Error sending raw packets.

#### Type Aliases
- **`socket::raw::PacketBuffer`**: Type alias for raw packet buffer.
- **`socket::raw::PacketMetadata`**: Type alias for raw packet metadata.

#### Module: `smoltcp::socket::dhcpv4` (DHCPv4 Client)

#### Structs
- **`socket::dhcpv4::Config`**: DHCPv4 client configuration.
- **`socket::dhcpv4::RetryConfig`**: DHCPv4 retry configuration.
- **`socket::dhcpv4::ServerInfo`**: DHCP server information.
- **`socket::dhcpv4::Socket`**: DHCPv4 client socket.

#### Enums
- **`socket::dhcpv4::Event`**: DHCPv4 event.

#### Module: `smoltcp::socket::dns` (DNS Client)

#### Structs
- **`socket::dns::DnsQuery`**: DNS query builder.
- **`socket::dns::QueryHandle`**: Handle to a DNS query.
- **`socket::dns::Socket`**: DNS client socket.

#### Enums
- **`socket::dns::GetQueryResultError`**: Error getting DNS query result.
- **`socket::dns::MulticastDns`**: Multicast DNS configuration.
- **`socket::dns::StartQueryError`**: Error starting DNS query.

#### Traits
- **`socket::AnySocket`**: Trait for any socket type.

### Module: `smoltcp::storage` (Storage/Buffer Types)

#### Structs
- **`storage::Assembler`**: Packet assembler for fragmented packets.
- **`storage::Empty`**: Empty storage marker.
- **`storage::Full`**: Full storage marker.
- **`storage::PacketBuffer`**: Generic packet buffer.
- **`storage::PacketMetadata`**: Generic packet metadata storage.
- **`storage::RingBuffer`**: Ring buffer for no-alloc usage.

#### Traits
- **`storage::Resettable`**: Trait for resettable storage.

### Module: `smoltcp::time` (Time Types)

#### Structs
- **`time::Duration`**: Time duration.
- **`time::Instant`**: A point in time (used for polling).

### Module: `smoltcp::wire` (Wire Protocol / Packet Types)

#### Structs
- **`wire::ArpPacket`**: ARP packet.
- **`wire::DhcpFlags`**: DHCP flags.
- **`wire::DhcpOption`**: DHCP option.
- **`wire::DhcpOptionWriter`**: DHCP option writer.
- **`wire::DhcpPacket`**: DHCP packet.
- **`wire::DhcpRepr`**: DHCP packet representation.
- **`wire::DnsFlags`**: DNS flags.
- **`wire::DnsPacket`**: DNS packet.
- **`wire::DnsQuestion`**: DNS question.
- **`wire::DnsRecord`**: DNS record.
- **`wire::DnsRepr`**: DNS packet representation.
- **`wire::Error`**: Wire protocol error.
- **`wire::EthernetAddress`**: MAC address (Ethernet).
- **`wire::EthernetFrame`**: Ethernet frame.
- **`wire::EthernetRepr`**: Ethernet frame representation.
- **`wire::Icmpv4Packet`**: ICMPv4 packet.
- **`wire::IgmpPacket`**: IGMP packet.
- **`wire::IpEndpoint`**: IP address + port endpoint.
- **`wire::IpListenEndpoint`**: Listening endpoint (IP + port or port only).
- **`wire::Ipv4Address`**: IPv4 address.
- **`wire::Ipv4Cidr`**: IPv4 CIDR block.
- **`wire::Ipv4FragKey`**: IPv4 fragmentation key.
- **`wire::Ipv4Packet`**: IPv4 packet.
- **`wire::Ipv4Repr`**: IPv4 packet representation.
- **`wire::RawHardwareAddress`**: Raw hardware address.
- **`wire::TcpPacket`**: TCP packet.
- **`wire::TcpRepr`**: TCP packet representation.
- **`wire::TcpSeqNumber`**: TCP sequence number.
- **`wire::TcpTimestampRepr`**: TCP timestamp representation.
- **`wire::UdpPacket`**: UDP packet.
- **`wire::UdpRepr`**: UDP packet representation.
- **`wire::pretty_print::PrettyIndent`**: Pretty printer indentation helper.
- **`wire::pretty_print::PrettyPrinter`**: Pretty printer for packet debugging.

#### Enums
- **`wire::ArpHardware`**: ARP hardware type.
- **`wire::ArpOperation`**: ARP operation type.
- **`wire::ArpRepr`**: ARP packet representation.
- **`wire::DhcpMessageType`**: DHCP message type.
- **`wire::DhcpOpCode`**: DHCP operation code.
- **`wire::DnsOpcode`**: DNS operation code.
- **`wire::DnsQueryType`**: DNS query type (A, AAAA, etc.).
- **`wire::DnsRcode`**: DNS response code.
- **`wire::DnsRecordData`**: DNS record data.
- **`wire::EthernetProtocol`**: Ethernet protocol type.
- **`wire::HardwareAddress`**: Hardware address type.
- **`wire::IcmpRepr`**: ICMP packet representation.
- **`wire::Icmpv4DstUnreachable`**: ICMPv4 destination unreachable code.
- **`wire::Icmpv4Message`**: ICMPv4 message type.
- **`wire::Icmpv4ParamProblem`**: ICMPv4 parameter problem code.
- **`wire::Icmpv4Redirect`**: ICMPv4 redirect code.
- **`wire::Icmpv4Repr`**: ICMPv4 packet representation.
- **`wire::Icmpv4TimeExceeded`**: ICMPv4 time exceeded code.
- **`wire::IgmpRepr`**: IGMP packet representation.
- **`wire::IgmpVersion`**: IGMP version.
- **`wire::IpAddress`**: IP address (IPv4 or IPv6).
- **`wire::IpCidr`**: IP CIDR block (IPv4 or IPv6).
- **`wire::IpProtocol`**: IP protocol number.
- **`wire::IpRepr`**: IP packet representation.
- **`wire::IpVersion`**: IP version (IPv4 or IPv6).
- **`wire::TcpControl`**: TCP control flags.
- **`wire::TcpOption`**: TCP option type.

#### Traits
- **`wire::pretty_print::PrettyPrint`**: Trait for pretty-printing packets.

#### Type Aliases
- **`wire::Result`**: Result type for wire operations.
- **`wire::TcpTimestampGenerator`**: TCP timestamp generator function type.

#### Constants
- **`wire::DHCP_CLIENT_PORT`**: DHCP client port (68).
- **`wire::DHCP_MAX_DNS_SERVER_COUNT`**: Maximum DNS servers in DHCP response.
- **`wire::DHCP_SERVER_PORT`**: DHCP server port (67).
- **`wire::ETHERNET_HEADER_LEN`**: Ethernet header length.
- **`wire::IPV4_HEADER_LEN`**: IPv4 header length.
- **`wire::IPV4_MIN_MTU`**: Minimum IPv4 MTU.
- **`wire::IPV4_MULTICAST_ALL_ROUTERS`**: IPv4 multicast address for all routers.
- **`wire::IPV4_MULTICAST_ALL_SYSTEMS`**: IPv4 multicast address for all systems.
- **`wire::MAX_HARDWARE_ADDRESS_LEN`**: Maximum hardware address length.
- **`wire::TCP_HEADER_LEN`**: TCP header length.
- **`wire::UDP_HEADER_LEN`**: UDP header length.

### Module: `smoltcp::config` (Configuration Constants)

#### Constants
- **`config::ASSEMBLER_MAX_SEGMENT_COUNT`**: Maximum segments in packet assembler.
- **`config::DNS_MAX_NAME_SIZE`**: Maximum DNS name size.
- **`config::DNS_MAX_RESULT_COUNT`**: Maximum DNS results.
- **`config::DNS_MAX_SERVER_COUNT`**: Maximum DNS servers.
- **`config::FRAGMENTATION_BUFFER_SIZE`**: Fragmentation buffer size.
- **`config::IFACE_MAX_ADDR_COUNT`**: Maximum interface addresses.
- **`config::IFACE_MAX_MULTICAST_GROUP_COUNT`**: Maximum multicast groups.
- **`config::IFACE_MAX_ROUTE_COUNT`**: Maximum routes.
- **`config::IFACE_MAX_SIXLOWPAN_ADDRESS_CONTEXT_COUNT`**: Maximum 6LoWPAN address contexts.
- **`config::IFACE_NEIGHBOR_CACHE_COUNT`**: Neighbor cache size.
- **`config::IPV6_HBH_MAX_OPTIONS`**: Maximum IPv6 hop-by-hop options.
- **`config::REASSEMBLY_BUFFER_COUNT`**: Number of reassembly buffers.
- **`config::REASSEMBLY_BUFFER_SIZE`**: Reassembly buffer size.
- **`config::RPL_PARENTS_BUFFER_COUNT`**: RPL parents buffer count.
- **`config::RPL_RELATIONS_BUFFER_COUNT`**: RPL relations buffer count.

