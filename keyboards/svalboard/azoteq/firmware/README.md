# TPS43 (IQS572-B000) settings image for 0.7 mm glass

`IQS572_B000_58_15_2_2_Settings_V0.5_BL_Svalboard_TPS43_20260424.hex` is the Azoteq GUI
export prepared by Georg Visagie (Azoteq applications engineering) on 2026-04-24 after
bonding a TPS43 to 0.7 mm glass with the supplied 3M 468MP adhesive and characterising it on
a CT210A. It is the stock TPS43 B000 v2.2 firmware plus these settings changes:

| Register | Setting | Stock | This image | Why |
|---|---|---|---|---|
| 0x0596 | Global touch multiplier, set | ~16 (87 counts) | **32** (175 counts) | glass gives ~2x deltas (700-800 counts / 8 mm touch) |
| 0x0597 | Global touch multiplier, clear | ~12 (65 counts) | **24** (131 counts) | hysteresis scaled with it |
| 0x0580 | LP1 report rate | 80 ms | **50 ms** | ALP channel tracks environment better than TP channels |
| 0x0586 | Idle mode timeout | 10 s | **2 s** | enter ALP mode sooner after the hand leaves |

Unchanged but relevant: ATI target 700, global ATI C 1, prox threshold 23, palm reject on,
export version (reg 0x0677) = **5**.

## How it gets onto a module

`azoteq.c` reads the module identity at boot. If it is an IQS5xx-B000 with a bootloader and
its settings export version is not 5, `iqs5xx_bootloader.c` reflashes it over I2C (~4 s,
once per module). The runtime-writable registers above are re-applied every boot regardless,
because the stock QMK driver init resets the idle timeout.

Disable auto-flash with `#define SVAL_IQS_AUTOFLASH false`; force a reflash from code with
`sval_iqs5xx_flash_image()`. Enable `CONSOLE_ENABLE` to see the identity / flash log.

## Regenerating the C image

```
python hex2c.py <export.hex> iqs5xx_fw_image.h
```

The image spans 0x83C0..0xBFFF (15424 bytes) with 0x00 where the hex has no record, matching
the Linux `iqs5xx` driver. The settings block sits at flash `0xB92B + <I2C register address>`
inside the image, which is how `hex2c.py` extracts the export version and thresholds.

## Provenance

- Modules: TPS43, Mouser order 33282803 (Aug 2024), shipped without glass.
- Glass: 0.7 mm, 1000 pcs from Azoteq's factory; the original samples from Lev Popov
  (Orbital) came pre-laminated and never showed the false-click / drift behaviour.
- The Orbital firmware's own runtime settings (ATI C 1, ATI target 700, prox 24) match the
  stock TPS43 defaults; its touch multipliers (8/6) went the *other* way and are not glass
  calibration.
- Azoteq contacts: Francois Bruwer (sales eng), Georg Visagie (apps), Ryan Terry (FAE, via
  Alex Monetta at Meridian Tech).
