# `embassy-net` (0.7.1)

## What it does in this repo

Async networking stack integration commonly used alongside `smoltcp`.

This repo includes `embassy-net` with features:

- `dhcpv4`, `tcp`, `udp`
- `medium-ethernet`
- `log`

## Key links

- API docs (docs.rs): https://docs.rs/embassy-net/0.7.1/embassy_net/
- Crate page: https://crates.io/crates/embassy-net
- Embassy book: https://embassy.dev/book/

## The “core” API surface (from the crate root)

The crate root lists the main types you’ll touch most often:

- `embassy_net::Config` (stack configuration)
- `embassy_net::StackResources` (static memory for the stack)
- `embassy_net::Stack` (handle you pass to sockets/tasks)
- `embassy_net::Runner` (the background “network task” you must run)

Crate root: https://docs.rs/embassy-net/0.7.1/embassy_net/

## How you typically use it

`embassy-net` is an async network stack designed for embedded systems (no_std, no_alloc) that builds on `smoltcp`:
https://docs.rs/embassy-net/0.7.1/embassy_net/

Typical steps:

1) Build/obtain a network **driver** that implements `embassy_net::driver` traits (often via a Wi‑Fi/Ethernet crate).
2) Create `Config` (DHCP or static config).
3) Allocate `StackResources` in static storage.
4) Call `embassy_net::new(...)` → get `(Stack, Runner)`.
5) Run `Runner` in a background task.
6) Create sockets from `embassy_net::tcp` / `embassy_net::udp` etc, using `Stack`.

## Code sketch: creating the stack and running the runner task

This shows the structure and the real entrypoints (`Config`, `Stack`, `Runner`, `StackResources`, `new`) as they appear in the docs. The **driver** is project-specific (Wi‑Fi/Ethernet).

```rust
// STRUCTURE EXAMPLE (driver specifics omitted)

use embassy_executor::Spawner;
use embassy_net::{Config, Runner, Stack, StackResources};
use static_cell::StaticCell;

// Pick a size that matches your socket usage (see StackResources docs).
static RESOURCES: StaticCell<StackResources<1>> = StaticCell::new();

#[embassy_executor::task]
async fn net_task(mut runner: Runner<'static>) -> ! {
    runner.run().await
}

async fn init_network(spawner: Spawner /*, driver: impl embassy_net::driver::Driver */) -> Stack<'static> {
    // DHCP is the common default for Wi‑Fi stations.
    let config = Config::dhcpv4(Default::default());

    let seed = 0x0123_4567_89ab_cdef; // stable-ish random seed

    // let (stack, runner) = embassy_net::new(driver, config, RESOURCES.init(StackResources::new()), seed);
    // spawner.spawn(net_task(runner)).unwrap();
    // stack

    todo!()
}
```

## Code sketch: TCP sockets (embassy-net::tcp)

`embassy-net` exposes TCP sockets under `embassy_net::tcp`:
https://docs.rs/embassy-net/0.7.1/embassy_net/tcp/index.html

```rust
// STRUCTURE EXAMPLE

use embassy_net::tcp::TcpSocket;
use embedded_io_async::{Read, Write};

async fn http_get_example(stack: embassy_net::Stack<'static>) {
    let mut rx_buf = [0u8; 2048];
    let mut tx_buf = [0u8; 2048];
    let mut socket = TcpSocket::new(stack, &mut rx_buf, &mut tx_buf);

    // socket.connect((ip, port)).await.unwrap();
    // socket.write_all(b"GET / HTTP/1.0\r\nHost: example.com\r\n\r\n").await.unwrap();
    // let n = socket.read(&mut rx_buf).await.unwrap();
    // ... process response ...
}
```

## In this repo (current state)

Depending on how you structure your Wi‑Fi bring-up, common entrypoints include:

- network stack types (stack/device/config)
- DHCP configuration
- TCP/UDP sockets

Note: this repo currently doesn’t set up the network stack yet—only the Wi‑Fi controller is initialized.

## Complete API inventory

Full API index: https://docs.rs/embassy-net/0.7.1/embassy_net/all.html

### Top-level Structs
- **`Config`**: Network stack configuration (DHCP/static, etc.).
- **`DhcpConfig`**: DHCP configuration options.
- **`EthernetAddress`**: MAC address type.
- **`IpEndpoint`**: IP address + port endpoint.
- **`IpListenEndpoint`**: Listening endpoint (IP + port or port only).
- **`Ipv4Address`**: IPv4 address.
- **`Ipv4Cidr`**: IPv4 CIDR block.
- **`Runner`**: Background driver that must be `.run().await`ed in a task.
- **`Stack`**: Handle used to create sockets and interact with the stack.
- **`StackResources<const N: usize>`**: Static memory backing for the stack (socket storage, etc.).
- **`StaticConfigV4`**: Static IPv4 configuration.

### Top-level Enums
- **`ConfigV4`**: IPv4 configuration (DHCP or static).
- **`HardwareAddress`**: Hardware address type (Ethernet or IEEE 802.15.4).
- **`IpAddress`**: IP address (IPv4 or IPv6).
- **`IpCidr`**: IP CIDR block (IPv4 or IPv6).

### Top-level Functions
- **`new(driver, config, resources, seed)`**: Constructs the stack and runner from a driver + config + resources + random seed.

### Modules

#### `embassy_net::tcp`
- **`TcpSocket`**: TCP socket implementing `embedded-io-async::Read` and `Write`.
- **`TcpReader`**: Read-only view of a TCP socket.
- **`TcpWriter`**: Write-only view of a TCP socket.
- **`tcp::client::TcpClient`**: High-level TCP client.
- **`tcp::client::TcpClientState`**: State for TCP client connections.
- **`tcp::client::TcpConnection`**: Active TCP connection.
- **`tcp::AcceptError`**: Error accepting connections.
- **`tcp::ConnectError`**: Error connecting.
- **`tcp::Error`**: General TCP error.
- **`tcp::State`**: TCP socket state.

#### `embassy_net::udp`
- **`UdpSocket`**: UDP socket.
- **`UdpMetadata`**: Metadata for UDP packets.
- **`udp::BindError`**: Error binding socket.
- **`udp::RecvError`**: Error receiving.
- **`udp::SendError`**: Error sending.
- **`udp::PacketMetadata`**: Type alias for UDP packet metadata.

