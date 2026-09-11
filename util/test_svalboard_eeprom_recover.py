#!/usr/bin/env python3
"""
Tests for svalboard_eeprom_recover.py.

Builds a synthetic wear-leveling backing store using an encoder written directly
from the LOG_ENTRY_MAKE_* macros in quantum/wear_leveling/wear_leveling_internal.h
and the write path in wear_leveling_write_raw(), then checks that the recovery
tool reads it back.

The central scenario reproduces the field failure: a single corrupted byte in an
*unused* part of the consolidated region breaks the whole-image FNV1a-64, the
firmware zeroes its cache and factory-resets the board, and the user's real
configuration is still sitting in flash afterwards.

Run: python3 util/test_svalboard_eeprom_recover.py
"""

import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import svalboard_eeprom_recover as rec  # noqa: E402

LOGICAL = rec.DEFAULT_LOGICAL_SIZE
BACKING = rec.DEFAULT_BACKING_SIZE
LOG_START = LOGICAL + 8

VIA_MAGIC_ADDR = 91        # EECONFIG_BASE_SIZE(37) + EECONFIG_KB_DATA_SIZE(54)
KEYMAP_ADDR = 95           # VIA_EEPROM_CONFIG_END
KEYMAP_SIZE = 16 * 10 * 6 * 2

USER_MAGIC = bytes([0xA1, 0xB2, 0xC3])
NEW_MAGIC = bytes([0xD4, 0xE5, 0xF6])


# ---------------------------------------------------------------------------
# Encoder: mirrors wear_leveling_write_raw() / LOG_ENTRY_MAKE_*
# ---------------------------------------------------------------------------

class LogWriter:
    def __init__(self):
        self.words = []
        self.entries = 0

    def _emit(self, word):
        self.words.append(word & 0xFFFF)

    def _emit_raw8(self, raw8, count):
        self.entries += 1
        for i in range(count):
            self._emit(raw8[2 * i] | (raw8[2 * i + 1] << 8))

    def write(self, address, data):
        remaining = len(data)
        pos = 0
        while remaining > 0:
            # Small-write optimization: uint16_t, value 0 or 1, even address < 16384
            if remaining >= 2 and address % 2 == 0 and address < 16384:
                value = (data[pos + 1] << 8) | data[pos]
                if value in (0, 1):
                    raw8 = bytearray(2)
                    raw8[0] = (rec.LOG_ENTRY_TYPE_WORD_01 << 6) \
                        | ((1 if value else 0) << 5) \
                        | ((address >> 9) & 0x1F)
                    raw8[1] = (address >> 1) & 0xFF
                    self._emit_raw8(raw8, 1)
                    remaining -= 2
                    address += 2
                    pos += 2
                    continue

            # Small-write optimization: address < 64
            if address < 64:
                raw8 = bytearray(2)
                raw8[0] = (rec.LOG_ENTRY_TYPE_OPTIMIZED_64 << 6) | (address & 0x3F)
                raw8[1] = data[pos]
                self._emit_raw8(raw8, 1)
                remaining -= 1
                address += 1
                pos += 1
                continue

            length = min(remaining, 5)
            raw8 = bytearray(8)
            raw8[0] = (rec.LOG_ENTRY_TYPE_MULTIBYTE << 6) \
                | ((length & 0x7) << 3) \
                | ((address >> 16) & 0x7)
            raw8[1] = (address >> 8) & 0xFF
            raw8[2] = address & 0xFF
            raw8[3:3 + length] = data[pos:pos + length]
            words = 2
            if length > 1:
                words = 3
            if length > 3:
                words = 4
            self._emit_raw8(raw8, words)
            remaining -= length
            address += length
            pos += length

    def to_bytes(self):
        return b"".join(w.to_bytes(2, "little") for w in self.words)


def build_backing(logical, log_bytes, corrupt_at=None):
    """Assemble a full backing store image, bit-inverted the way flash stores it."""
    image = bytearray(b"\xFF" * BACKING)
    consolidated = bytearray(logical)
    if corrupt_at is not None:
        consolidated[corrupt_at] ^= 0x01
    image[0:LOGICAL] = bytes(b ^ 0xFF for b in consolidated)
    # Checksum is over the *original* image, so corruption makes it mismatch.
    checksum = rec.fnv1a_64(bytes(logical))
    image[LOGICAL:LOGICAL + 8] = bytes(b ^ 0xFF for b in checksum.to_bytes(8, "little"))
    image[LOG_START:LOG_START + len(log_bytes)] = bytes(b ^ 0xFF for b in log_bytes)
    return bytes(image)


def user_keymap():
    return bytes(((0x0400 + i) % 0x10000).to_bytes(2, "big")[j]
                 for i in range(KEYMAP_SIZE // 2) for j in range(2))


def default_keymap():
    return bytes(((0x0900 + i) % 0x10000).to_bytes(2, "big")[j]
                 for i in range(KEYMAP_SIZE // 2) for j in range(2))


def make_scenario(corrupt_at=50000):
    """A board with a user config, then a factory reset appended to the log."""
    logical = bytearray(LOGICAL)
    logical[0:2] = (0xFEE6).to_bytes(2, "little")       # eeconfig magic
    logical[VIA_MAGIC_ADDR:VIA_MAGIC_ADDR + 3] = USER_MAGIC
    logical[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE] = user_keymap()

    writer = LogWriter()

    # Some ordinary user activity after the last consolidation.
    edited = bytearray(logical)
    edit = bytes([0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0])
    writer.write(KEYMAP_ADDR + 40, edit)
    edited[KEYMAP_ADDR + 40:KEYMAP_ADDR + 40 + len(edit)] = edit
    writer.write(60, bytes([0x07]))                     # kb datablock byte, opt64 path
    edited[60] = 0x07

    reset_entry_index = writer.entries

    # eeconfig_init_via(): via_eeprom_set_valid(false) writes 0xFF to each magic byte.
    for i in range(3):
        writer.write(VIA_MAGIC_ADDR + i, bytes([0xFF]))
    # dynamic_keymap_reset() rewrites the keymap from the compiled-in defaults.
    writer.write(KEYMAP_ADDR, default_keymap())
    # via_eeprom_set_valid(true) stamps the new firmware's magic.
    for i in range(3):
        writer.write(VIA_MAGIC_ADDR + i, bytes([NEW_MAGIC[i]]))

    image = build_backing(bytes(logical), writer.to_bytes(), corrupt_at=corrupt_at)
    return image, bytes(edited), reset_entry_index


# ---------------------------------------------------------------------------

class TestRoundTrip(unittest.TestCase):
    def test_encoder_decoder_round_trip(self):
        """Every write path (word01, opt64, multibyte) survives a decode."""
        logical = bytearray(LOGICAL)
        writer = LogWriter()
        cases = [
            (10, bytes([0xAB])),                        # opt64
            (1000, bytes([0x00, 0x00])),                # word01, value 0
            (2000, bytes([0x01, 0x00])),                # word01, value 1
            (5000, bytes(range(1, 6))),                 # multibyte, max length
            (20000, bytes(range(1, 4))),                # multibyte, length 3
            (30001, bytes([0x77])),                     # multibyte, odd address
            (40000, bytes(range(1, 12))),               # multi-chunk
        ]
        for addr, data in cases:
            writer.write(addr, data)
            logical[addr:addr + len(data)] = data

        image = build_backing(bytes(LOGICAL), writer.to_bytes())
        store = rec.BackingStore(image)
        self.assertEqual(store.log_status, "ok", store.log_error)
        replayed = store.replay()
        for addr, data in cases:
            self.assertEqual(replayed[addr:addr + len(data)], data,
                             "mismatch at 0x%04X" % addr)

    def test_checksum_matches_when_clean(self):
        image = build_backing(bytes(LOGICAL), b"")
        store = rec.BackingStore(image)
        self.assertTrue(store.checksum_ok)
        self.assertEqual(len(store.entries), 0)


class TestRecovery(unittest.TestCase):
    def setUp(self):
        self.image, self.expected, self.reset_index = make_scenario()
        self.store = rec.BackingStore(self.image)

    def test_corruption_breaks_checksum(self):
        """One flipped bit in unused space invalidates the entire 64 KiB image."""
        self.assertFalse(self.store.checksum_ok)

    def test_finds_reset_event(self):
        index, magic_addr = rec.find_reset_event(self.store)
        self.assertIsNotNone(index)
        self.assertEqual(magic_addr, VIA_MAGIC_ADDR)
        self.assertEqual(index, self.reset_index)

    def test_recovers_user_keymap(self):
        index, _ = rec.find_reset_event(self.store)
        recovered = self.store.replay(upto=index)
        self.assertEqual(recovered[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE],
                         self.expected[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE])
        self.assertEqual(recovered[VIA_MAGIC_ADDR:VIA_MAGIC_ADDR + 3], USER_MAGIC)
        self.assertNotEqual(recovered[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE],
                            default_keymap())

    def test_full_replay_shows_the_damage(self):
        """Replaying the whole log gives what the board actually boots with."""
        current = self.store.replay()
        self.assertEqual(current[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE], default_keymap())
        self.assertEqual(current[VIA_MAGIC_ADDR:VIA_MAGIC_ADDR + 3], NEW_MAGIC)

    def test_rebuild_produces_valid_image(self):
        index, _ = rec.find_reset_event(self.store)
        recovered = self.store.replay(upto=index)
        rebuilt = rec.build_backing_image(recovered, BACKING, LOGICAL)
        self.assertEqual(len(rebuilt), BACKING)

        reparsed = rec.BackingStore(rebuilt)
        self.assertTrue(reparsed.checksum_ok)
        self.assertEqual(len(reparsed.entries), 0)
        self.assertEqual(reparsed.replay(), recovered)

    def test_offset_autodetect_in_larger_dump(self):
        blob = bytearray(b"\xFF" * (2 * 1024 * 1024))
        blob[rec.DEFAULT_FLASH_BASE:rec.DEFAULT_FLASH_BASE + BACKING] = self.image
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as handle:
            handle.write(bytes(blob))
            path = handle.name
        try:
            store, offset = rec.load_backing(path, None, BACKING, LOGICAL)
            self.assertEqual(offset, rec.DEFAULT_FLASH_BASE)
            self.assertEqual(len(store.entries), len(self.store.entries))
        finally:
            os.unlink(path)


class TestBadLogEntry(unittest.TestCase):
    def test_out_of_range_entry_stops_playback(self):
        """A torn entry aborts the replay, matching wear_leveling_playback_log()."""
        writer = LogWriter()
        writer.write(1000, bytes([0xAA, 0xBB]))
        good = writer.to_bytes()
        # A multibyte header pointing past the end of the logical area.
        bad = bytes([(0 << 6) | (5 << 3) | 0x7, 0xFF, 0xFF, 0x00, 0, 0, 0, 0])
        image = build_backing(bytes(LOGICAL), good + bad)
        store = rec.BackingStore(image)
        self.assertEqual(store.log_status, "failed")
        self.assertEqual(len(store.entries), 1)


class TestCLI(unittest.TestCase):
    def setUp(self):
        self.image, self.expected, _ = make_scenario()
        self.dir = tempfile.mkdtemp()
        self.dump = os.path.join(self.dir, "dump.bin")
        with open(self.dump, "wb") as handle:
            handle.write(self.image)
        self.script = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "svalboard_eeprom_recover.py")

    def run_tool(self, *args):
        result = subprocess.run(
            [sys.executable, self.script] + list(args),
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_parse_reports_the_failure(self):
        out = self.run_tool("parse", self.dump)
        self.assertIn("MISMATCH", out)
        self.assertIn("factory reset      at log entry", out)
        self.assertIn("map confirmed", out)

    def test_restore_round_trip(self):
        output = os.path.join(self.dir, "restore.bin")
        self.run_tool("restore", self.dump, "-o", output)
        with open(output, "rb") as handle:
            rebuilt = handle.read()
        store = rec.BackingStore(rebuilt)
        self.assertTrue(store.checksum_ok)
        self.assertEqual(store.replay()[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE],
                         self.expected[KEYMAP_ADDR:KEYMAP_ADDR + KEYMAP_SIZE])

    def test_restore_with_magic_patch(self):
        output = os.path.join(self.dir, "restore_magic.bin")
        self.run_tool("restore", self.dump, "-o", output, "--magic", "11:22:33")
        with open(output, "rb") as handle:
            store = rec.BackingStore(handle.read())
        self.assertEqual(store.replay()[VIA_MAGIC_ADDR:VIA_MAGIC_ADDR + 3],
                         bytes([0x11, 0x22, 0x33]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
