#pragma once

#define ARTILLERY_PROTOCOL_VERSION 2
#define ARTILLERY_GAME_ID "artillery"
#define ARTILLERY_DISPLAY_WIDTH 480
#define ARTILLERY_DISPLAY_HEIGHT 320
#define ARTILLERY_MIN_PLAYERS 2
#define ARTILLERY_MAX_PLAYERS 2
#define ARTILLERY_DEFAULT_PORT 7420

#define ARTILLERY_UDP_MAGIC 0xA7
#define ARTILLERY_UDP_MAX_PACKET 1100
#define ARTILLERY_UDP_HEADER_SIZE 11
#define ARTILLERY_TOKEN_BYTES 8
#define ARTILLERY_ERROR_BYTES 24
#define ARTILLERY_WANT_BYTES 8

/*
 * UDP datagram protocol (little-endian, no JSON on the wire).
 *
 * Header (11 bytes):
 *   u8  magic   = ARTILLERY_UDP_MAGIC
 *   u8  version = ARTILLERY_PROTOCOL_VERSION
 *   u16 seq     packet sequence (never resent; next compose gets a new seq)
 *   u16 ack     highest packet seq received from peer
 *   u32 ack_bits bit n set => (ack - 1 - n) was received (n in 0..31)
 *   u8  msg_count
 *
 * Each message:
 *   u8  type
 *   u16 id      reliable message id (0 = unreliable, latest-wins per type)
 *   u16 len
 *   u8  payload[len]
 *
 * Reliability: unacked reliable messages are repeated in later datagrams until
 * a datagram that carried them is acked. Lost Fire/Impact recovers on the next
 * 20 Hz tick without TCP's retransmit timer.
 *
 * Message types (payload layouts in wire.hpp):
 *   Hello / Welcome / Error / Intent / Cmd / Aim / State / Sync
 *
 * Aim is unreliable (absolute angle+power). State carries snapshot fields plus
 * either full heights (join/resync) or crater column deltas (impact).
 *
 * Client sends intent only — never hit/win results. Authority owns Match.
 */
