// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Redundant copy of the settings that matter, kept outside the EEPROM region.
 *
 * The wear-leveling store holds exactly one copy of your configuration, and it
 * periodically erases and rewrites that copy in place. Lose power during that
 * window and the only copy is gone -- no checksum scheme can give it back. The
 * mirror exists so there is always a second copy that was not part of whatever
 * operation just failed.
 *
 * It lives in its own flash sectors below the EEPROM region, so it is never
 * erased by the same operation that writes the primary. It costs no EEPROM space
 * and no macro space.
 */

/**
 * Restores the primary store from the mirror if the primary has been lost.
 *
 * Must run after eeprom_driver_init() and before via_init() / quantum_init(), so
 * that a restored configuration is in place before anything decides to reset it.
 * keyboard_pre_init_kb() satisfies both.
 */
void sval_config_mirror_init(void);

/**
 * Refreshes the mirror when the live configuration has changed and the keyboard
 * is idle. Call from housekeeping_task_kb().
 */
void sval_config_mirror_task(void);

/** True if this boot restored the configuration from the mirror. */
bool sval_config_mirror_restored(void);

/** True if a usable mirror is currently on hand. */
bool sval_config_mirror_present(void);

/** Sequence number of the live mirror, or 0 if there is none. */
uint32_t sval_config_mirror_sequence(void);
