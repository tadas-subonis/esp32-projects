"""Minimal artillery UDP v2 codec for Python server tests."""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

MAGIC = 0xA7
VERSION = 2
HDR = struct.Struct("<BBHHIB")  # magic, version, seq, ack, ack_bits, msg_count
MSG_HDR = struct.Struct("<BHH")  # type, id, len

Hello = 1
Welcome = 2
Error = 3
Intent = 4
Cmd = 5
Aim = 6
State = 7
Sync = 8

# ClientIntent::Kind
KIND_START = 1
KIND_FIRE = 8
KIND_SET_ANGLE = 4
KIND_SET_POWER = 5

WANT = {"bot": 0, "hotseat": 1, "pvp": 2}
WANT_NAME = {0: "bot", 1: "hotseat", 2: "pvp"}
PHASE = {0: "title", 1: "aiming", 2: "firing", 3: "resolving", 4: "gameover"}
MODE = {0: "bot", 1: "hotseat", 2: "pvp"}


def pack_packet(seq: int, ack: int, ack_bits: int, messages: List[Tuple[int, int, bytes]]) -> bytes:
    body = bytearray()
    for typ, mid, payload in messages:
        body += MSG_HDR.pack(typ, mid & 0xFFFF, len(payload))
        body += payload
    return HDR.pack(MAGIC, VERSION, seq & 0xFFFF, ack & 0xFFFF, ack_bits & 0xFFFFFFFF, len(messages)) + bytes(
        body
    )


def unpack_packet(data: bytes) -> Optional[Tuple[int, int, int, List[Tuple[int, int, bytes]]]]:
    if len(data) < HDR.size:
        return None
    magic, ver, seq, ack, ack_bits, count = HDR.unpack_from(data, 0)
    if magic != MAGIC or ver != VERSION:
        return None
    off = HDR.size
    msgs: List[Tuple[int, int, bytes]] = []
    for _ in range(count):
        if off + MSG_HDR.size > len(data):
            return None
        typ, mid, ln = MSG_HDR.unpack_from(data, off)
        off += MSG_HDR.size
        if off + ln > len(data):
            return None
        msgs.append((typ, mid, data[off : off + ln]))
        off += ln
    return seq, ack, ack_bits, msgs


def encode_hello(want: str = "bot", token: str = "", seq: int = 0, eid: int = 0, t_ms: int = 0) -> bytes:
    tok = token.encode("ascii")[:8].ljust(8, b"\0")
    return struct.pack("<BB", VERSION, WANT.get(want, 0)) + tok + struct.pack("<III", seq, eid, t_ms)


def encode_intent(kind: int, value: int = 0, seed: int = 0) -> bytes:
    return struct.pack("<BhI", kind & 0xFF, value, seed & 0xFFFFFFFF)


def encode_aim(angle: int, power: int, who: int = 0) -> bytes:
    return struct.pack("<BBB", who & 0xFF, angle & 0xFF, power & 0xFF)


def decode_welcome(payload: bytes) -> dict:
    seat_u, want, seq = struct.unpack_from("<BBI", payload, 0)
    seat = struct.unpack("<b", bytes([seat_u]))[0]
    token = payload[6:14].split(b"\0", 1)[0].decode("ascii", errors="replace")
    eid, t_ms = struct.unpack_from("<II", payload, 14)
    return {
        "type": "welcome",
        "ok": True,
        "seat": seat,
        "want": WANT_NAME.get(want, "bot"),
        "seq": seq,
        "token": token,
        "eid": eid,
        "t_ms": t_ms,
    }


def decode_error(payload: bytes) -> dict:
    cmd = payload[0]
    err = payload[1:25].split(b"\0", 1)[0].decode("ascii", errors="replace")
    return {"type": "error", "ok": False, "cmd": cmd, "error": err}


def decode_state(payload: bytes) -> dict:
    # 23 fields / 44 bytes before optional heights or deltas (9x i16 including tank_y1).
    fmt = "<IBBIBBBBhhhhhhhhhBBBBii"
    vals = struct.unpack_from(fmt, payload, 0)
    seq, phase, mode, seed, turn, active, _winner, flags = vals[0:8]
    angle, power, wind = vals[8:11]
    angle0, angle1, players, title_sel = vals[17:21]
    return {
        "type": "state",
        "ok": True,
        "seq": seq,
        "phase": PHASE.get(phase, str(phase)),
        "mode": MODE.get(mode, str(mode)),
        "seed": seed,
        "turn": turn,
        "active": active,
        "angle": angle,
        "power": power,
        "wind": wind,
        "angle0": angle0,
        "angle1": angle1,
        "players": players,
        "title_sel": title_sel,
        "flags": flags,
    }


def decode_cmd(payload: bytes) -> dict:
    seq, who, kind, value, seed = struct.unpack_from("<IBBhI", payload, 0)
    kind_name = {
        0: "select",
        1: "start",
        2: "rematch",
        3: "title",
        4: "angle",
        5: "power",
        6: "nudge_angle",
        7: "nudge_power",
        8: "fire",
    }.get(kind, str(kind))
    return {"type": "cmd", "seq": seq, "who": who, "cmd": kind_name, "value": value, "seed": seed}


def decode_message(typ: int, payload: bytes) -> Optional[dict]:
    if typ == Welcome:
        return decode_welcome(payload)
    if typ == Error:
        return decode_error(payload)
    if typ == State:
        return decode_state(payload)
    if typ == Cmd:
        return decode_cmd(payload)
    return {"type": typ, "len": len(payload)}


def _seq_greater(a: int, b: int) -> bool:
    return ((a - b) & 0xFFFF) < 32768 and a != b


@dataclass
class UdpSession:
    local_seq: int = 0
    remote_seq: int = 0
    have_remote: bool = False
    next_msg_id: int = 1
    next_deliver: int = 1
    pending: List[Tuple[int, int, bytes]] = field(default_factory=list)
    packet_msgs: Dict[int, List[int]] = field(default_factory=dict)
    unreliable: Optional[Tuple[int, bytes]] = None
    delivered: List[dict] = field(default_factory=list)
    recv_buf: Dict[int, Tuple[int, bytes]] = field(default_factory=dict)
    recv_seen: Dict[int, bool] = field(default_factory=dict)

    def send_reliable(self, typ: int, payload: bytes) -> None:
        mid = self.next_msg_id
        self.next_msg_id = 1 if (mid + 1) & 0xFFFF == 0 else mid + 1
        self.pending.append((mid, typ, payload))

    def send_unreliable(self, typ: int, payload: bytes) -> None:
        self.unreliable = (typ, payload)

    def compose(self) -> bytes:
        self.local_seq = (self.local_seq + 1) & 0xFFFF
        msgs: List[Tuple[int, int, bytes]] = []
        ids: List[int] = []
        for mid, typ, payload in self.pending:
            msgs.append((typ, mid, payload))
            ids.append(mid)
        if self.unreliable is not None:
            typ, payload = self.unreliable
            msgs.append((typ, 0, payload))
            self.unreliable = None
        self.packet_msgs[self.local_seq] = ids
        ack = self.remote_seq if self.have_remote else 0
        bits = 0
        if self.have_remote:
            for i in range(32):
                s = (self.remote_seq - 1 - i) & 0xFFFF
                if self.recv_seen.get(s):
                    bits |= 1 << i
        return pack_packet(self.local_seq, ack, bits, msgs)

    def _ack_packet(self, pkt: int) -> None:
        ids = self.packet_msgs.pop(pkt, [])
        if not ids:
            return
        idset = set(ids)
        self.pending = [p for p in self.pending if p[0] not in idset]

    def pump(self, data: bytes) -> None:
        parsed = unpack_packet(data)
        if parsed is None:
            return
        seq, ack, ack_bits, messages = parsed
        self.remote_seq = seq
        self.have_remote = True
        self.recv_seen[seq] = True
        self._ack_packet(ack)
        for i in range(32):
            if ack_bits & (1 << i):
                self._ack_packet((ack - 1 - i) & 0xFFFF)

        for typ, mid, payload in messages:
            if mid == 0:
                msg = decode_message(typ, payload)
                if msg:
                    self.delivered.append(msg)
                continue
            if mid != self.next_deliver and not _seq_greater(mid, self.next_deliver):
                continue
            self.recv_buf[mid] = (typ, payload)
            while self.next_deliver in self.recv_buf:
                typ2, payload2 = self.recv_buf.pop(self.next_deliver)
                msg = decode_message(typ2, payload2)
                if msg:
                    self.delivered.append(msg)
                self.next_deliver = 1 if (self.next_deliver + 1) & 0xFFFF == 0 else self.next_deliver + 1
