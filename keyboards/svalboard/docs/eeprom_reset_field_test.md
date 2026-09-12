# Configuration loss: cause and fix

Some Svalboards have come back from an unplug, a KVM switch, or a laptop waking up
with their layout reset to factory defaults — macros gone, mouse settings reset,
layers wrong. This describes why, and what changed.

## Why it happened

Your configuration is stored in one place, with one integrity check covering the
whole thing. Every so often the keyboard rewrites that storage from scratch. The
rewrite erases the old copy first and takes roughly a second, during which it
cannot be interrupted.

Lose power in that window and the only copy of your configuration is damaged or
gone. On the next power-up the keyboard cannot verify what it finds, concludes the
data is garbage, and overwrites it with the defaults compiled into the firmware.

Two things made this unrecoverable:

1. **One copy.** Nothing else held your layout, so once the rewrite was interrupted
   there was nothing to fall back on.
2. **One integrity check over everything.** A single bad byte anywhere — including
   in the ~63 KiB of macro space that is mostly empty — invalidated the whole
   store, keymap included.

## What this build changes

**A second copy, kept somewhere safer.** The settings that matter — core config,
the VIA identity bytes, and the full dynamic keymap, about 2 KiB — are mirrored
into two dedicated flash sectors that sit outside the EEPROM region entirely. They
are never erased by the operation that rewrites the primary, so an interrupted
rewrite can no longer take both.

On startup, if the primary store has been lost, the keyboard restores from the
mirror before anything upstream gets a chance to decide the store is uninitialised
and reset it. Losing your layout now requires two independent failures.

This costs **no EEPROM space and no macro space**. It uses 8 KiB of the roughly
1.8 MiB of program flash that sits unused between the firmware (~78 KiB) and the
EEPROM region at the top of the chip. The slots are written alternately, so the
mirror is never the only copy while one of them is mid-erase, and each carries its
own hash so a half-written slot is simply ignored.

The mirror is refreshed only when the live configuration has actually changed and
the keyboard has been idle for a few seconds — writing it costs an erase plus a
program with interrupts disabled, so it must not land while someone is typing.

**Second: damaged no longer means disposable.** If the integrity check fails but
there is clearly real data present, the keyboard now keeps it and flags the boot
rather than erasing. An erased store is still detected as such, so a genuine first
boot initializes normally.

## What you would see

Press your status key with a text editor focused. The readout ends with an `NVM:`
line:

```
NVM: ok | checksum stored ... computed ... | 412 log entries replayed | mirror ok (seq 7)
```

Healthy. `mirror ok` means a good backup is on hand.

Two things are worth reporting if you ever see them:

- `CONFIG RESTORED FROM MIRROR` — the primary store was lost and the backup put it
  back. This is the failure happening and being caught.
- `SUSPECT - contents preserved, NOT reset to defaults` — the integrity check
  failed and the data was kept rather than erased.

Either line means something we want to know about. The state is recomputed each
boot and is not stored, so copy it before unplugging.

`no mirror yet` on a freshly flashed board is normal; the mirror is written the
first time the keyboard is idle.

## Limitations

- The macro buffer is not mirrored. It is 97% of the store, and mirroring it would
  mean constantly rewriting 63 KiB for little gain. Macros are still lost if the
  primary is destroyed. If that matters, keep a Vial backup.
- If the corruption lands on the few bytes that identify the store as yours, and
  the mirror is also unavailable, a reset can still occur.
- A store that is genuinely scrambled with no valid mirror is cleared the way it
  always was: **reflash the firmware**. Flashing any freshly built image gives it a
  new identity, the keyboard sees that the stored data belongs to a different build,
  and it resets itself. The protection described above deliberately steps aside when
  that happens, because reflashing is an explicit request for a fresh start. A
  reset key would still be a convenience, but it is not the only way out.
- The underlying one-second rewrite window is unchanged. The mirror makes it
  survivable; it does not make it shorter. Shrinking it means moving the flash
  layout, which needs its own migration plan.

## If you need to recover or investigate

Dump the affected half in BOOTSEL mode:

```
picotool save -r 0x101E0000 0x10200000 dump.bin -f
```

```
python3 util/svalboard_eeprom_recover.py parse dump.bin
```

reports whether the checksum matched, how full the write log was, and whether a
factory reset is recorded. `restore` can repackage a recovered configuration into
a flashable image.

Verify the firmware itself against the release artifact with
`picotool verify <file>.uf2`. A damaged firmware image would not run at all — the
board would appear as a USB drive named RPI-RP2 rather than a keyboard.

## Tests

```
./util/wear_leveling_test/run.sh                     # host test of the storage behaviour
python3 util/test_svalboard_eeprom_recover.py        # tests for the recovery tool
```

`run.sh` compiles the real storage layer against an in-memory backing store.
Building it with `-DWEAR_LEVELING_ZERO_ON_CORRUPTION` restores the old behaviour
and the corruption tests then fail.
