// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* In-firmware flasher for the Azoteq IQS5xx-B000 I2C bootloader (IQS550/572/525,
 * i.e. the TPS43 / TPS65 modules).  Programs the whole application + settings map
 * (0x83C0..0xBFFF) that the Azoteq GUI exports in "BL" hex format.
 *
 * Protocol (same as the Linux drivers/input/touchscreen/iqs5xx.c implementation):
 *   bootloader I2C address = application address XOR 0x40  (0x74 -> 0x34)
 *   0x00 VER  : write cmd, read 2 bytes           -> 0x0200
 *   <addr><64 bytes> : raw block write, big-endian address, 64-byte blocks
 *   0x03 CRC  : write cmd, wait 50 ms, read 1 byte -> 0x00 = pass
 *   0x01 READ : write cmd + 2-byte address, read 64 bytes (used to verify)
 *   0x02 EXEC : write cmd                          -> jump to application
 * The bootloader is only reachable for ~2 ms after a reset; with no NRST GPIO on the
 * Svalboard PCB we use the application's own software reset (System Control 1) and
 * poll the VER command immediately afterwards.
 */

typedef enum {
    IQS5XX_BL_OK = 0,
    IQS5XX_BL_ERR_NO_BOOTLOADER, /* could not reach the bootloader after reset */
    IQS5XX_BL_ERR_WRITE,         /* a 64-byte block write failed repeatedly */
    IQS5XX_BL_ERR_CRC,           /* bootloader CRC check failed after programming */
    IQS5XX_BL_ERR_VERIFY,        /* read-back of the settings region mismatched */
    IQS5XX_BL_ERR_EXEC,          /* EXEC command not acknowledged */
} iqs5xx_bl_result_t;

/* Application-mode identity of the attached module. All fields 0 on read failure. */
typedef struct {
    uint16_t product_number;    /* 40 = IQS550, 58 = IQS572, 52 = IQS525 */
    uint16_t project_number;    /* 15 = generic B000 */
    uint8_t  major, minor;
    uint8_t  bootloader_status; /* 0xA5 = bootloader available, 0xEE = none */
    uint16_t export_version;    /* reg 0x0677: GUI "export file version" of the settings in flash */
} iqs5xx_identity_t;

bool               iqs5xx_read_identity(iqs5xx_identity_t *id);
iqs5xx_bl_result_t iqs5xx_bl_program(const uint8_t *image, uint16_t image_len);
const char        *iqs5xx_bl_result_str(iqs5xx_bl_result_t r);
