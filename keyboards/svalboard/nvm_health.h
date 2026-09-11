// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * Reports whether this boot found the stored configuration in a questionable
 * state. True means the firmware declined to reinitialise storage that it could
 * not vouch for, and the settings currently in use may be incomplete.
 */
bool sval_nvm_degraded(void);

/**
 * Writes a one-line human-readable summary of the storage state into buf.
 *
 * This is what a field tester reads back: if a board ever "randomly reset to
 * defaults", the line tells us whether the storage layer detected the fault, and
 * whether the configuration was preserved instead of being overwritten.
 */
void sval_nvm_health_string(char *buf, size_t len);

/**
 * Emits the same summary to the debug console at startup.
 */
void sval_nvm_health_report(void);
