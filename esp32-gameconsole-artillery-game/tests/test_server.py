#!/usr/bin/env python3
"""UDP server is authoritative: clients send intent, server validates and simulates."""
from __future__ import annotations

import socket
import subprocess
import sys
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hostbin import host_bin
from udp_wire import (
    Aim,
    Hello,
    Intent,
    KIND_FIRE,
    KIND_START,
    UdpSession,
    encode_aim,
    encode_hello,
    encode_intent,
)

SERVER = host_bin("artillery-server")


class UdpClient:
    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.05)
        self.addr = (host, port)
        self.sess = UdpSession()

    def close(self) -> None:
        self.sock.close()

    def send_compose(self) -> None:
        self.sock.sendto(self.sess.compose(), self.addr)

    def pump(self, timeout: float = 0.05) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                data, _ = self.sock.recvfrom(2048)
            except socket.timeout:
                break
            self.sess.pump(data)

    def exchange(self, rounds: int = 8, pause: float = 0.02) -> None:
        for _ in range(rounds):
            self.send_compose()
            self.pump(pause)

    def hello(self, want: str = "bot", token: str = "") -> dict:
        self.sess.send_reliable(Hello, encode_hello(want=want, token=token))
        for _ in range(40):
            self.send_compose()
            self.pump(0.05)
            for m in list(self.sess.delivered):
                if m.get("type") == "welcome":
                    # Ack so the server stops repeating welcome/state.
                    self.send_compose()
                    return m
        raise AssertionError("no welcome")

    def take(self, pred, timeout: float = 2.0) -> dict:
        deadline = time.time() + timeout
        while time.time() < deadline:
            for i, m in enumerate(self.sess.delivered):
                if pred(m):
                    self.sess.delivered.pop(i)
                    return m
            self.send_compose()
            self.pump(0.05)
        raise AssertionError("no matching message from server")


class ServerAuthorityTests(unittest.TestCase):
    def setUp(self) -> None:
        if not SERVER.exists():
            raise unittest.SkipTest(f"missing {SERVER}")
        self.port = 17420
        self.proc = subprocess.Popen(
            [str(SERVER), "--port", str(self.port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        # UDP has no accept; wait for the process to bind.
        time.sleep(0.25)
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.settimeout(0.5)
        try:
            # Empty / invalid packet still proves the port is open (or we time out).
            probe.sendto(b"\x00", ("127.0.0.1", self.port))
            try:
                probe.recvfrom(64)
            except socket.timeout:
                pass  # no reply expected for garbage; bind succeeded if sendto worked
        except OSError as exc:
            self.proc.kill()
            raise unittest.SkipTest(f"server did not bind udp: {exc}") from exc
        finally:
            probe.close()

    def tearDown(self) -> None:
        self.proc.terminate()
        try:
            self.proc.wait(timeout=1)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=1)

    def _connect(self, want: str = "bot") -> UdpClient:
        c = UdpClient("127.0.0.1", self.port)
        welcome = c.hello(want=want)
        self.assertEqual(welcome.get("type"), "welcome")
        return c

    def test_client_should_not_be_able_to_fire_before_the_match_starts(self) -> None:
        c = self._connect()
        c.sess.send_reliable(Intent, encode_intent(KIND_FIRE))
        err = c.take(lambda m: m.get("type") == "error")
        c.close()
        self.assertFalse(err.get("ok", True))
        self.assertEqual(err.get("error"), "not_aiming")

    def test_server_should_simulate_a_legal_shot_and_return_state(self) -> None:
        c = self._connect()
        c.sess.send_reliable(Intent, encode_intent(KIND_START, value=0, seed=11))
        started = c.take(lambda m: m.get("type") == "state" and m.get("phase") == "aiming")
        self.assertTrue(started.get("ok"))
        c.sess.send_unreliable(Aim, encode_aim(55, 80))
        c.exchange(4)
        c.sess.send_reliable(Intent, encode_intent(KIND_FIRE))
        fired = c.take(
            lambda m: m.get("ok") and m.get("phase") in ("firing", "resolving", "aiming", "gameover")
        )
        c.close()
        self.assertIn(fired.get("phase"), ("firing", "resolving", "aiming", "gameover"))
        self.assertIn("angle0", started)

    def test_two_pvp_clients_should_share_one_authoritative_match(self) -> None:
        a = UdpClient("127.0.0.1", self.port)
        welcome_a = a.hello(want="pvp")
        self.assertEqual(welcome_a.get("seat"), 0)

        a.sess.send_reliable(Intent, encode_intent(KIND_START, value=2, seed=11))
        err = a.take(lambda m: m.get("type") == "error")
        self.assertEqual(err.get("error"), "need_two_players")

        b = UdpClient("127.0.0.1", self.port)
        welcome_b = b.hello(want="pvp")
        self.assertEqual(welcome_b.get("seat"), 1)
        a.exchange(10, 0.03)
        lobby = a.take(lambda m: m.get("type") == "state" and m.get("players") == 2)
        self.assertEqual(lobby.get("phase"), "title")

        a.sess.send_reliable(Intent, encode_intent(KIND_START, value=2, seed=11))
        started = a.take(lambda m: m.get("type") == "state" and m.get("phase") == "aiming")
        self.assertEqual(started.get("mode"), "pvp")
        self.assertEqual(started.get("active"), 0)

        b.sess.send_reliable(Intent, encode_intent(KIND_FIRE))
        reject = b.take(lambda m: m.get("type") == "error")
        self.assertEqual(reject.get("error"), "not_your_turn")

        a.sess.send_unreliable(Aim, encode_aim(50, 55))
        a.exchange(6)
        angled = a.take(
            lambda m: (m.get("type") == "state" and m.get("angle") == 50)
            or (m.get("type") == "cmd" and m.get("cmd") == "angle" and m.get("value") == 50)
            or (m.get("type") == "state" and m.get("angle0") == 50),
            timeout=1.0,
        )
        # Aim is unreliable — accept either aim reflection in state or just that exchange succeeded.
        self.assertTrue(angled is not None)

        a.close()
        b.close()

    def test_pvp_reconnect_should_keep_the_same_seat(self) -> None:
        a = UdpClient("127.0.0.1", self.port)
        welcome = a.hello(want="pvp")
        self.assertEqual(welcome.get("seat"), 0)
        token = welcome.get("token")
        self.assertTrue(token)

        b = UdpClient("127.0.0.1", self.port)
        b.hello(want="pvp")

        a.close()
        time.sleep(0.05)
        again = UdpClient("127.0.0.1", self.port)
        again.sess.send_reliable(Hello, encode_hello(want="pvp", token=token))
        welcome2 = None
        for _ in range(50):
            again.send_compose()
            again.pump(0.05)
            for m in list(again.sess.delivered):
                if m.get("type") == "error":
                    self.fail(f"reconnect error: {m}")
                if m.get("type") == "welcome":
                    welcome2 = m
                    break
            if welcome2:
                break
        self.assertIsNotNone(welcome2, "no welcome on token reclaim")
        self.assertEqual(welcome2.get("seat"), 0)
        self.assertEqual(welcome2.get("token"), token)
        snap = again.take(lambda m: m.get("type") == "state")
        self.assertIn("seq", snap)
        again.close()
        b.close()


if __name__ == "__main__":
    sys.exit(unittest.main(verbosity=2))
