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

## API inventory (high-level)

For the complete list of items, use the “All Items” page in rustdoc (recommended for searching):
- https://docs.rs/smoltcp/0.12.0/smoltcp/all.html

- **Modules**
  - **`smoltcp::socket`**: socket implementations + state machines (TCP/UDP/ICMP/raw) and their buffers.
  - **`smoltcp::iface`**: network interface logic (polling, routing packets between device and sockets).
  - **`smoltcp::phy`**: the “device” abstraction layer (how smoltcp talks to your NIC/Wi‑Fi driver).
  - **`smoltcp::wire`**: packet parsing/encoding + IP/MAC address types.
  - **`smoltcp::time`**: time types (`Instant`, etc.) used when polling.
  - **`smoltcp::storage`**: specialized containers for no-alloc embedded usage.

- **Common structs/types you’ll see in examples**
  - **`smoltcp::iface::Interface`**: the central interface object you poll.
  - **`smoltcp::iface::SocketSet`**: collection storing sockets (no heap required if you provide storage).
  - **`smoltcp::socket::tcp::Socket`**: TCP socket state machine.
  - **`smoltcp::socket::tcp::SocketBuffer`**: explicit RX/TX buffers for TCP sockets.
  - **`smoltcp::wire::IpAddress` / `IpCidr`**: IP addresses and CIDR blocks.

