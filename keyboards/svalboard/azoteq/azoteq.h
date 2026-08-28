// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include "iqs5xx_bootloader.h"

/* Identity of the attached TPS43 as read at init (zeros if the read failed). */
const iqs5xx_identity_t *sval_iqs5xx_identity(void);

/* Reflash the bundled settings image (firmware/iqs5xx_fw_image.h) into the module,
 * regardless of the version it currently carries, then re-run driver init.
 * Blocks for ~4 s. Returns true if the bootloader reported success. */
bool sval_iqs5xx_flash_image(void);

/* Re-read identity (up to `attempts` tries) and, if now readable, run the
 * auto-flash check and re-apply runtime tuning. */
void sval_iqs5xx_refresh(uint8_t attempts);

/* Whether the most recent identity read attempt succeeded (false = the cached
 * identity is being shown). */
bool sval_iqs5xx_link_ok(void);

/* Settings export version bundled in this firmware (from the Azoteq hex). */
uint16_t sval_iqs5xx_expected_version(void);

/* "not needed" if no flash was attempted this boot, else the bootloader result. */
const char *sval_iqs5xx_flash_status_str(void);
