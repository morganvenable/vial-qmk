#!/usr/bin/env python3
"""
Recover a Svalboard (RP2040 + QMK wear-leveling) EEPROM image from a raw flash dump.

Background
----------
Svalboard builds use EEPROM_DRIVER=vendor on RP2040, which resolves to QMK's
wear-leveling driver backed by the RP2040's QSPI flash (see
builddefs/common_features.mk and platforms/chibios/drivers/wear_leveling/).

The backing store is laid out as:

    [0, LOGICAL)                  consolidated image  (the "cache" snapshot)
    [LOGICAL, LOGICAL+8)          FNV1a-64 of the consolidated image
    [LOGICAL+8, BACKING)          append-only write log

All values are stored bit-inverted, so erased flash (0xFF) reads back as 0x0000
and is treated as "empty log slot".

On boot, wear_leveling_read_consolidated() hashes the whole consolidated image
and compares it against the stored checksum. On ANY mismatch it calls
wear_leveling_clear_cache(), which memsets the entire 64 KiB cache to zero.
Every subsequent eeprom_read_byte() then returns 0x00, the VIA magic check in
via_eeprom_is_valid() fails, and eeconfig_init_via() overwrites the user's
keymap with the compiled-in defaults.

Crucially, that overwrite is *appended to the log* -- it does not erase the
consolidated region. So the user's real configuration is usually still sitting
in flash, intact, and can be read straight back out. That is what this tool does.

Usage
-----
Dump the backing region from a board in BOOTSEL mode (do this per half):

    picotool save -r 0x101E0000 0x10200000 dump.bin -f

Then:

    svalboard_eeprom_recover.py parse   dump.bin
    svalboard_eeprom_recover.py restore dump.bin -o restore.bin

And write it back (board in BOOTSEL mode):

    picotool load restore.bin -o 0x101E0000 -f

No dependencies beyond the standard library.
"""

import argparse
import os
import sys

# ---------------------------------------------------------------------------
# Wear-leveling format
# ---------------------------------------------------------------------------

DEFAULT_BACKING_SIZE = 128 * 1024   # keyboards/svalboard/config.h
DEFAULT_LOGICAL_SIZE = 64 * 1024    # BACKING/2, per wear_leveling_rp2040_flash_config.h

# WEAR_LEVELING_RP2040_FLASH_BASE = PICO_FLASH_SIZE_BYTES - WEAR_LEVELING_BACKING_SIZE.
# PICO_FLASH_SIZE_BYTES comes from lib/pico-sdk/.../boards/pico.h and is 2 MiB.
DEFAULT_FLASH_BASE = (2 * 1024 * 1024) - DEFAULT_BACKING_SIZE   # 0x1E0000

FNV1A_64_INIT = 0xCBF29CE484222325
FNV_64_PRIME = 0x100000001B3

LOG_ENTRY_TYPE_MULTIBYTE = 0
LOG_ENTRY_TYPE_OPTIMIZED_64 = 1
LOG_ENTRY_TYPE_WORD_01 = 2

TYPE_NAMES = {0: "multibyte", 1: "opt64", 2: "word01", 3: "invalid"}


def fnv1a_64(buf):
    h = FNV1A_64_INIT
    for b in buf:
        h ^= b
        h = (h * FNV_64_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


class LogEntry:
    """One decoded write-log entry."""

    def __init__(self, index, offset, etype, address, data, words):
        self.index = index
        self.offset = offset        # byte offset within the backing store
        self.etype = etype
        self.address = address      # logical EEPROM address written
        self.data = data            # bytes written
        self.words = words          # number of 16-bit words the entry occupies

    def __repr__(self):
        return "[%4d] @0x%05X %-9s addr=0x%04X len=%d data=%s" % (
            self.index, self.offset, TYPE_NAMES.get(self.etype, "?"),
            self.address, len(self.data), self.data.hex()
        )


class ParseError(Exception):
    pass


class BackingStore:
    """A parsed wear-leveling backing store."""

    def __init__(self, raw, logical_size=DEFAULT_LOGICAL_SIZE):
        self.logical_size = logical_size
        self.backing_size = len(raw)
        if self.backing_size < logical_size * 2:
            raise ParseError(
                "backing region (%d bytes) is smaller than twice the logical size (%d)"
                % (self.backing_size, logical_size)
            )
        self.raw = raw

        # Everything is stored inverted; invert byte-wise to get logical values.
        self.consolidated = bytes(b ^ 0xFF for b in raw[0:logical_size])
        self.stored_checksum = int.from_bytes(
            bytes(b ^ 0xFF for b in raw[logical_size:logical_size + 8]), "little"
        )
        self.computed_checksum = fnv1a_64(self.consolidated)

        self.log_start = logical_size + 8
        self.entries = []
        self.log_status = "ok"      # ok | failed
        self.log_error = None
        self.write_address = self.log_start
        self._parse_log()

    # -- log ---------------------------------------------------------------

    def _word(self, offset):
        return (~int.from_bytes(self.raw[offset:offset + 2], "little")) & 0xFFFF

    def _parse_log(self):
        offset = self.log_start
        index = 0
        while offset < self.backing_size:
            start = offset
            w0 = self._word(offset)
            if w0 == 0:
                # Empty slot: end of log. This is what the firmware does too.
                break
            offset += 2

            raw8 = bytearray(8)
            raw8[0:2] = w0.to_bytes(2, "little")
            etype = (raw8[0] >> 6) & 0x3

            try:
                if etype == LOG_ENTRY_TYPE_MULTIBYTE:
                    offset = self._need(offset, 2)
                    raw8[2:4] = self._word(offset).to_bytes(2, "little")
                    offset += 2
                    addr = ((raw8[0] & 0x7) << 16) | (raw8[1] << 8) | raw8[2]
                    length = (raw8[0] >> 3) & 0x7
                    if addr + length > self.logical_size:
                        raise ParseError(
                            "multibyte entry at 0x%05X targets 0x%04X+%d, past logical end"
                            % (start, addr, length)
                        )
                    if length > 1:
                        offset = self._need(offset, 2)
                        raw8[4:6] = self._word(offset).to_bytes(2, "little")
                        offset += 2
                    if length > 3:
                        offset = self._need(offset, 2)
                        raw8[6:8] = self._word(offset).to_bytes(2, "little")
                        offset += 2
                    data = bytes(raw8[3:3 + length])

                elif etype == LOG_ENTRY_TYPE_OPTIMIZED_64:
                    addr = raw8[0] & 0x3F
                    data = bytes([raw8[1]])
                    if addr >= self.logical_size:
                        raise ParseError("opt64 entry at 0x%05X targets 0x%04X" % (start, addr))

                elif etype == LOG_ENTRY_TYPE_WORD_01:
                    addr = ((raw8[0] & 0x1F) << 9) | (raw8[1] << 1)
                    value = (raw8[0] >> 5) & 0x1
                    data = bytes([value, 0])
                    if addr + 1 >= self.logical_size:
                        raise ParseError("word01 entry at 0x%05X targets 0x%04X" % (start, addr))

                else:
                    raise ParseError("unknown entry type %d at 0x%05X" % (etype, start))

            except ParseError as exc:
                # The firmware treats this as WEAR_LEVELING_FAILED and force-consolidates
                # whatever it has replayed so far (wear_leveling.c:604-607).
                self.log_status = "failed"
                self.log_error = str(exc)
                break

            self.entries.append(LogEntry(index, start, etype, addr, data, (offset - start) // 2))
            index += 1

        self.write_address = offset

    def _need(self, offset, count):
        if offset + count > self.backing_size:
            raise ParseError("entry runs past end of backing store at 0x%05X" % offset)
        return offset

    # -- replay ------------------------------------------------------------

    def replay(self, upto=None):
        """Rebuild the logical image, applying log entries [0, upto)."""
        cache = bytearray(self.consolidated)
        entries = self.entries if upto is None else self.entries[:upto]
        for entry in entries:
            a = entry.address
            cache[a:a + len(entry.data)] = entry.data
        return bytes(cache)

    @property
    def checksum_ok(self):
        return self.stored_checksum == self.computed_checksum


# ---------------------------------------------------------------------------
# Factory-reset detection
# ---------------------------------------------------------------------------

def find_reset_event(store):
    """
    Locate the factory reset in the write log.

    eeconfig_init_via() (quantum/via.c:142-152) begins by calling
    via_eeprom_set_valid(false), which writes 0xFF to each of the three VIA magic
    bytes at VIA_EEPROM_MAGIC_ADDR + 0/1/2. Those land in the log as three
    single-byte writes to consecutive addresses. That triplet is a reliable
    marker for "the firmware started wiping the config here", and it also tells us
    VIA_EEPROM_MAGIC_ADDR empirically, with no need to guess the address map.

    Returns (entry_index, magic_addr) or (None, None).
    """
    singles = [
        (i, e.address) for i, e in enumerate(store.entries)
        if len(e.data) == 1 and e.data[0] == 0xFF
    ]
    for pos in range(len(singles) - 2):
        (i0, a0), (i1, a1), (i2, a2) = singles[pos], singles[pos + 1], singles[pos + 2]
        # Must be consecutive log entries writing consecutive addresses.
        if i1 == i0 + 1 and i2 == i0 + 2 and a1 == a0 + 1 and a2 == a0 + 2:
            return i0, a0
    return None, None


# ---------------------------------------------------------------------------
# EEPROM region map
# ---------------------------------------------------------------------------

class RegionMap:
    """
    Mirrors the address arithmetic in quantum/nvm/eeprom/nvm_eeprom_eeconfig_internal.h,
    nvm_eeprom_via_internal.h and nvm_dynamic_keymap.c.

    Only used for the human-readable report. The recovery and restore paths work on
    the raw logical image and do not depend on any of this being exactly right.
    """

    EECONFIG_BASE_SIZE = 37   # sizeof(eeprom_core_t), packed

    def __init__(self, kb_data_size=54, user_data_size=0, layers=16, rows=10, cols=6,
                 layout_options_size=1, custom_config_size=0):
        self.eeconfig_size = self.EECONFIG_BASE_SIZE + kb_data_size + user_data_size
        self.kb_datablock = self.EECONFIG_BASE_SIZE
        self.kb_data_size = kb_data_size
        self.via_magic = self.eeconfig_size
        self.layout_options = self.via_magic + 3
        self.custom_config = self.layout_options + layout_options_size
        self.via_config_end = self.custom_config + custom_config_size
        self.keymap = self.via_config_end
        self.keymap_size = layers * rows * cols * 2
        self.layers, self.rows, self.cols = layers, rows, cols
        # Encoders / QMK settings / tap dance / combo / key override are all
        # zero-sized in the shipping svalboard vial build, so the macro buffer
        # follows the keymap directly. Reported as "approximate" for that reason.
        self.after_keymap = self.keymap + self.keymap_size

    def describe(self):
        return [
            ("eeconfig core",      0, self.EECONFIG_BASE_SIZE),
            ("kb datablock",       self.kb_datablock, self.kb_data_size),
            ("VIA magic",          self.via_magic, 3),
            ("VIA layout options", self.layout_options, 1),
            ("dynamic keymap",     self.keymap, self.keymap_size),
        ]


# ---------------------------------------------------------------------------
# Keycode names (best effort)
# ---------------------------------------------------------------------------

def load_keycode_names(repo_root):
    """Best-effort keycode value -> name map from QMK's constant data."""
    names = {}
    path = os.path.join(repo_root, "data", "constants", "keycodes")
    if not os.path.isdir(path):
        return names
    try:
        import hjson as parser
    except ImportError:
        try:
            import json as parser
        except ImportError:
            return names
    for filename in sorted(os.listdir(path)):
        if not filename.endswith(".hjson"):
            continue
        try:
            with open(os.path.join(path, filename), "r", encoding="utf-8") as handle:
                blob = parser.load(handle)
        except Exception:
            continue
        for key, spec in (blob.get("keycodes") or {}).items():
            try:
                names[int(key, 16)] = spec.get("key") or spec.get("label") or key
            except (ValueError, AttributeError):
                continue
    return names


def keycode_str(value, names):
    name = names.get(value)
    return name if name else "0x%04X" % value


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------

def load_backing(path, offset, backing_size, logical_size):
    with open(path, "rb") as handle:
        blob = handle.read()

    if offset is None:
        candidates = []
        if len(blob) == backing_size:
            candidates = [0]
        else:
            candidates = [DEFAULT_FLASH_BASE, len(blob) - backing_size, 0]
        chosen, best = None, None
        for candidate in candidates:
            if candidate < 0 or candidate + backing_size > len(blob):
                continue
            try:
                probe = BackingStore(blob[candidate:candidate + backing_size], logical_size)
            except ParseError:
                continue
            # Prefer a region whose checksum validates, then one whose log parsed cleanly.
            score = (probe.checksum_ok, probe.log_status == "ok", len(probe.entries) > 0)
            if best is None or score > best:
                chosen, best = candidate, score
        if chosen is None:
            raise ParseError(
                "could not locate a wear-leveling region in %s; pass --offset explicitly "
                "(0x%X is where this firmware puts it)" % (path, DEFAULT_FLASH_BASE)
            )
        offset = chosen

    if offset + backing_size > len(blob):
        raise ParseError(
            "dump is %d bytes; need %d at offset 0x%X"
            % (len(blob), backing_size, offset)
        )
    return BackingStore(blob[offset:offset + backing_size], logical_size), offset


def build_backing_image(logical, backing_size, logical_size):
    """Build a flashable backing-store image: consolidated data + checksum + empty log."""
    if len(logical) != logical_size:
        raise ParseError("logical image is %d bytes, expected %d" % (len(logical), logical_size))
    image = bytearray(b"\xFF" * backing_size)
    image[0:logical_size] = bytes(b ^ 0xFF for b in logical)
    checksum = fnv1a_64(logical)
    image[logical_size:logical_size + 8] = bytes(
        b ^ 0xFF for b in checksum.to_bytes(8, "little")
    )
    # Remainder stays 0xFF, which reads back as 0x0000 == empty log slot.
    return bytes(image)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def summarize(store, offset, args, out=sys.stdout):
    w = out.write
    w("Backing store\n")
    w("  source offset      0x%06X (%d bytes)\n" % (offset, store.backing_size))
    w("  logical size       %d bytes\n" % store.logical_size)
    w("  stored checksum    0x%016X\n" % store.stored_checksum)
    w("  computed checksum  0x%016X\n" % store.computed_checksum)
    if store.checksum_ok:
        w("  checksum           OK\n")
    else:
        w("  checksum           *** MISMATCH ***\n")
        w("                     This is the failure that makes the firmware zero its\n")
        w("                     64 KiB RAM cache and factory-reset the board, even though\n")
        w("                     the consolidated data below is still readable.\n")

    used = store.write_address - store.log_start
    capacity = store.backing_size - store.log_start
    w("  log entries        %d (%d/%d bytes, %.1f%% of log space)\n"
      % (len(store.entries), used, capacity, 100.0 * used / capacity))
    if store.log_status != "ok":
        w("  log parse          FAILED: %s\n" % store.log_error)
        w("                     The firmware responds to this by force-consolidating\n")
        w("                     immediately, which erases the whole backing store.\n")

    index, magic_addr = find_reset_event(store)
    if index is None:
        w("  factory reset      not found in log\n")
    else:
        w("  factory reset      at log entry %d (offset 0x%05X)\n"
          % (index, store.entries[index].offset))
        w("  VIA magic address  0x%04X (derived from the reset marker)\n" % magic_addr)

    regions = RegionMap(kb_data_size=args.kb_data_size, layers=args.layers,
                        rows=args.rows, cols=args.cols)
    w("\nEEPROM region map (computed)\n")
    for name, addr, size in regions.describe():
        w("  %-20s 0x%04X .. 0x%04X  (%d bytes)\n" % (name, addr, addr + size - 1, size))

    if magic_addr is not None:
        if magic_addr == regions.via_magic:
            w("  cross-check        computed VIA magic 0x%04X matches the log marker: map confirmed\n"
              % regions.via_magic)
        else:
            w("  cross-check        *** computed VIA magic 0x%04X != log marker 0x%04X ***\n"
              % (regions.via_magic, magic_addr))
            w("                     The region map above is wrong for this firmware.\n")
            w("                     Recovery and restore are unaffected; the decode below is not.\n")

    if index is not None:
        before = store.replay(upto=index)
        after = store.replay()
        differing = sum(1 for x, y in zip(before, after) if x != y)
        w("\nImpact of the reset\n")
        w("  bytes changed      %d\n" % differing)
        km, size = regions.keymap, regions.keymap_size
        km_diff = sum(1 for x, y in zip(before[km:km + size], after[km:km + size]) if x != y)
        w("  keymap bytes lost  %d of %d\n" % (km_diff, size))
    return index


def decode_keymap(logical, regions, names, out=sys.stdout):
    w = out.write
    base = regions.keymap
    w("\nDynamic keymap\n")
    for layer in range(regions.layers):
        rows = []
        nonempty = False
        for row in range(regions.rows):
            cells = []
            for col in range(regions.cols):
                off = base + (layer * regions.rows * regions.cols * 2) \
                    + (row * regions.cols * 2) + (col * 2)
                value = int.from_bytes(logical[off:off + 2], "big")
                if value:
                    nonempty = True
                cells.append(keycode_str(value, names))
            rows.append("    " + " ".join("%-14s" % c for c in cells).rstrip())
        if nonempty:
            w("  layer %d\n" % layer)
            w("\n".join(rows) + "\n")


def cmd_parse(args):
    store, offset = load_backing(args.dump, args.offset, args.backing_size, args.logical_size)
    index = summarize(store, offset, args)

    if args.entries:
        print("\nWrite log")
        for entry in store.entries:
            marker = "  <-- factory reset starts here" if entry.index == index else ""
            print("  %s%s" % (entry, marker))

    if args.keymap:
        regions = RegionMap(kb_data_size=args.kb_data_size, layers=args.layers,
                            rows=args.rows, cols=args.cols)
        names = load_keycode_names(args.repo_root)
        logical = store.replay(upto=index)
        decode_keymap(logical, regions, names)
    return 0


def _recovered_image(store, args):
    index, _ = find_reset_event(store)
    if args.at_entry is not None:
        upto = args.at_entry
        note = "log replayed up to entry %d (explicit --at-entry)" % upto
    elif args.full_log:
        upto = None
        note = "full log replayed (current on-board state, including the reset)"
    elif index is not None:
        upto = index
        note = "log replayed up to entry %d, just before the factory reset" % index
    else:
        upto = None
        note = "full log replayed (no factory reset found in the log)"
    return store.replay(upto=upto), note


def cmd_recover(args):
    store, offset = load_backing(args.dump, args.offset, args.backing_size, args.logical_size)
    logical, note = _recovered_image(store, args)
    with open(args.output, "wb") as handle:
        handle.write(logical)
    print("Wrote %d-byte logical EEPROM image to %s" % (len(logical), args.output))
    print("  %s" % note)
    return 0


def cmd_rebuild(args):
    with open(args.image, "rb") as handle:
        logical = handle.read()
    image = build_backing_image(logical, args.backing_size, args.logical_size)
    with open(args.output, "wb") as handle:
        handle.write(image)
    print("Wrote %d-byte flashable backing image to %s" % (len(image), args.output))
    print("  picotool load %s -o 0x%08X -f" % (args.output, 0x10000000 + DEFAULT_FLASH_BASE))
    return 0


def cmd_restore(args):
    store, offset = load_backing(args.dump, args.offset, args.backing_size, args.logical_size)
    summarize(store, offset, args)
    logical, note = _recovered_image(store, args)

    if args.magic:
        try:
            magic = bytes(int(x, 16) for x in args.magic.replace(":", " ").split())
        except ValueError:
            print("error: --magic must be three hex bytes, e.g. 'A1:B2:C3'", file=sys.stderr)
            return 2
        if len(magic) != 3:
            print("error: --magic must be exactly three bytes", file=sys.stderr)
            return 2
        _, magic_addr = find_reset_event(store)
        if magic_addr is None:
            regions = RegionMap(kb_data_size=args.kb_data_size, layers=args.layers,
                                rows=args.rows, cols=args.cols)
            magic_addr = regions.via_magic
        patched = bytearray(logical)
        patched[magic_addr:magic_addr + 3] = magic
        logical = bytes(patched)
        print("\nPatched VIA magic at 0x%04X to %s" % (magic_addr, magic.hex(":")))

    image = build_backing_image(logical, args.backing_size, args.logical_size)
    with open(args.output, "wb") as handle:
        handle.write(image)
    print("\nWrote %d-byte flashable backing image to %s" % (len(image), args.output))
    print("  %s" % note)
    print("\nTo write it back, put the half into BOOTSEL and run:")
    print("  picotool load %s -o 0x%08X -f" % (args.output, 0x10000000 + DEFAULT_FLASH_BASE))
    print("\nNote: the recovered image carries the VIA magic from the firmware that wrote it.")
    print("Restore it under that same firmware build, or pass --magic to retarget it,")
    print("otherwise the running firmware will treat it as stale and reset it again.")
    return 0


def main(argv=None):
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--backing-size", type=lambda v: int(v, 0), default=DEFAULT_BACKING_SIZE)
    parser.add_argument("--logical-size", type=lambda v: int(v, 0), default=DEFAULT_LOGICAL_SIZE)
    parser.add_argument("--offset", type=lambda v: int(v, 0), default=None,
                        help="offset of the backing region within the dump (auto-detected)")
    parser.add_argument("--kb-data-size", type=int, default=54,
                        help="EECONFIG_KB_DATA_SIZE (default: 54)")
    parser.add_argument("--layers", type=int, default=16)
    parser.add_argument("--rows", type=int, default=10)
    parser.add_argument("--cols", type=int, default=6)
    parser.set_defaults(repo_root=repo_root, at_entry=None, full_log=False)

    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("parse", help="analyze a dump and report what happened")
    p.add_argument("dump")
    p.add_argument("--entries", action="store_true", help="list every write-log entry")
    p.add_argument("--keymap", action="store_true", help="decode the recovered dynamic keymap")
    p.set_defaults(func=cmd_parse)

    p = sub.add_parser("recover", help="write the recovered logical EEPROM image")
    p.add_argument("dump")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--at-entry", type=int, help="replay the log only up to this entry index")
    p.add_argument("--full-log", action="store_true",
                   help="replay the entire log (current on-board state)")
    p.set_defaults(func=cmd_recover)

    p = sub.add_parser("rebuild", help="turn a logical EEPROM image into a flashable backing image")
    p.add_argument("image")
    p.add_argument("-o", "--output", required=True)
    p.set_defaults(func=cmd_rebuild)

    p = sub.add_parser("restore", help="parse + recover + rebuild in one step")
    p.add_argument("dump")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--at-entry", type=int, help="replay the log only up to this entry index")
    p.add_argument("--full-log", action="store_true")
    p.add_argument("--magic", help="rewrite the VIA magic bytes, e.g. 'A1:B2:C3'")
    p.set_defaults(func=cmd_restore)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except ParseError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
