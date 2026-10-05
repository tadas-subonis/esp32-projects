#!/usr/bin/env python3
"""Host CLI tests for artillery-sim. Run after `cmake --build build-host`."""
from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hostbin import host_bin

SIM = host_bin("artillery-sim")


def run(*args: str) -> dict:
    if not SIM.exists():
        raise unittest.SkipTest(f"missing {SIM}; build host first")
    proc = subprocess.run([str(SIM), *args], check=True, capture_output=True, text=True)
    return json.loads(proc.stdout.strip().splitlines()[-1])


class SimCliTests(unittest.TestCase):
    def test_state_should_start_a_match_in_aiming(self) -> None:
        # given a known seed
        seed = "11"
        # when
        data = run("state", "--seed", seed)
        # then
        self.assertTrue(data["ok"])
        self.assertEqual(data["phase"], "aiming")
        self.assertEqual(data["hp0"], 100)
        self.assertEqual(data["hp1"], 100)
        self.assertLess(data["tank0"][0], data["tank1"][0])

    def test_fire_should_return_an_impact(self) -> None:
        data = run("fire", "--seed", "11", "--player", "0", "--angle", "55", "--power", "80")
        self.assertTrue(data["ok"])
        self.assertGreater(data["steps"], 5)
        self.assertTrue(data["hit_terrain"] or data["left_map"] or data["hit_tank0"] or data["hit_tank1"])

    def test_same_seed_should_be_deterministic(self) -> None:
        a = run("fire", "--seed", "3", "--angle", "30", "--power", "55", "--wind", "2")
        b = run("fire", "--seed", "3", "--angle", "30", "--power", "55", "--wind", "2")
        self.assertEqual(a["impact_x"], b["impact_x"])
        self.assertEqual(a["impact_y"], b["impact_y"])
        self.assertEqual(a["steps"], b["steps"])


if __name__ == "__main__":
    sys.exit(unittest.main(verbosity=2))
