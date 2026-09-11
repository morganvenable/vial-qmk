# "My board randomly reset to defaults" — field test build

This branch changes how the firmware reacts when it cannot verify the stored
configuration, and adds a way to tell whether that ever happened to you. If you
have had a Svalboard lose its layout for no apparent reason, this is the build to
run.

## What causes it

Svalboard uses an RP2040 with no dedicated EEPROM chip. `EEPROM_DRIVER` defaults
to `vendor`, which on RP2040 resolves to QMK's wear-leveling driver backed by the
QSPI flash (`builddefs/common_features.mk`, `WEAR_LEVELING_DRIVER = rp2040_flash`).
The top 128 KiB of flash is laid out as:

```
[0, 64K)          consolidated image      the settings snapshot
[64K, 64K+8)      FNV1a-64 checksum       one hash over the whole 64 KiB
[64K+8, 128K)     write log               appended changes since the last snapshot
```

At boot, `wear_leveling_read_consolidated()` hashes the entire 64 KiB and compares
it against the stored checksum. Because there is a single hash covering everything,
**one wrong byte anywhere invalidates all of it** — including bytes in regions
nothing ever reads.

Before this change, a mismatch called `wear_leveling_clear_cache()`, which zeroes
the whole 64 KiB RAM cache. From that point on:

1. Every `eeprom_read_byte()` returns `0x00`.
2. `via_eeprom_is_valid()` fails, so `via_init()` calls `eeconfig_init_via()` and
   rewrites the keymap from the firmware's compiled-in defaults.
3. `eeconfig_is_enabled()` fails too, so `quantum_init()` calls `eeconfig_init()`,
   which starts with `nvm_eeconfig_erase()` — a **full 128 KiB flash erase**.

Step 3 is what makes this unrecoverable: the original data is physically erased
during the same boot that mishandled it. The board comes back on the compiled-in
default layout, which is why it reads as a clean factory reset rather than as
corruption.

## What this build changes

An unreadable configuration and an invalid one are now treated as different
things (`wear_leveling_integrity_t`):

| State | Meaning | Action |
|---|---|---|
| `OK` | Checksum matched | Normal boot |
| `BLANK` | Backing store is erased | Normal first-boot initialization |
| `SUSPECT` | Checksum failed but data is present | **Keep the data, do not reset** |

When the state is `SUSPECT`, the cache is no longer zeroed, and both destructive
paths are suppressed: `via_init()` skips `eeconfig_init_via()`, and `quantum_init()`
skips `eeconfig_init()` and its erase. The reasoning is the asymmetry — a bad
checksum means *at least one* byte is wrong, while zeroing the cache guarantees
*every* byte is wrong and then invites the layers above to erase the flash.

`BLANK` is detected by checking whether the consolidated area is entirely zero, so
a genuine first boot after an erase still initializes normally.

## How to tell whether it happened to you

Press your **status key** (`SV_OUTPUT_STATUS`) with a text editor focused. The
board types out its status, now with a final `NVM:` line:

```
NVM: ok | checksum stored 1E9AF793... computed 1E9AF793... | 412 log entries replayed
```

That is a healthy board. If you ever see this instead:

```
NVM: SUSPECT - contents preserved, NOT reset to defaults | checksum stored ... computed ... | 412 log entries replayed
```

then you have hit the exact condition that used to wipe boards, and this firmware
caught it and kept your configuration. **Please report that line.**

`| WRITE LOG TRUNCATED` appearing anywhere in the line is also worth reporting: it
means a write-log entry could not be decoded.

The state is recomputed each boot and is not stored, so capture it before
rebooting.

## If you see SUSPECT, please also dump your flash

This lets us confirm the mechanism against real hardware rather than a simulated
fault. Put the affected half into BOOTSEL (unplug, hold the boot button, plug in)
and run:

```
picotool save -r 0x101E0000 0x10200000 dump.bin -f
```

Then send `dump.bin`. You can inspect it yourself first:

```
python3 util/svalboard_eeprom_recover.py parse dump.bin
```

That reports whether the checksum matched, how full the write log was, and whether
a factory reset is recorded in the log.

## Recovering a board that already lost its layout

If your board was wiped by an **older** firmware, the data is almost certainly gone
— `nvm_eeconfig_erase()` erased it during that boot. A dump is still worth taking
for diagnosis, but do not expect recovery.

If a board running **this** firmware reports `SUSPECT`, the data was preserved and
`util/svalboard_eeprom_recover.py` can extract and repackage it:

```
python3 util/svalboard_eeprom_recover.py restore dump.bin -o restore.bin
picotool load restore.bin -o 0x101E0000 -f
```

## Known limitations

- A board in the `SUSPECT` state stays that way until the write log fills and the
  driver consolidates on its own, which rewrites the checksum and clears the state.
  Until then, automatic resets stay suppressed — including the one that normally
  fires after a firmware update, since Vial derives its EEPROM magic from a
  randomly generated `BUILD_ID` (`util/build_id.py`). That is deliberate for a test
  build: it keeps the evidence and your layout, and it fails safe.
- The 128 KiB `WEAR_LEVELING_BACKING_SIZE` in `keyboards/svalboard/config.h` is 16x
  the QMK default. Consolidation therefore erases 128 KiB and rewrites 64 KiB with
  interrupts disabled — a window of roughly a second where power loss destroys
  everything. This build does not change that, because changing the backing size
  moves the flash region and would itself lose every user's configuration. It is
  worth revisiting separately.
- Nothing here addresses the root cause of the corruption; it makes the firmware
  stop amplifying a single bad byte into total loss.

## Tests

```
./util/wear_leveling_test/run.sh                     # host test of the C behaviour
python3 util/test_svalboard_eeprom_recover.py        # tests for the recovery tool
```

`run.sh` compiles the real `quantum/wear_leveling/wear_leveling.c` against an
in-memory backing store. Building it with `-DWEAR_LEVELING_ZERO_ON_CORRUPTION`
restores the old behaviour, and the corruption tests then fail — which is the
difference this branch makes, in one command.
