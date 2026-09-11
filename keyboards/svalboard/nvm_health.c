// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>

#include "nvm_health.h"
#include "print.h"

#ifdef WEAR_LEVELING_ENABLE
#    include "wear_leveling.h"
#endif // WEAR_LEVELING_ENABLE

bool sval_nvm_degraded(void) {
#ifdef WEAR_LEVELING_ENABLE
    const wear_leveling_report_t *report = wear_leveling_report();
    return report->integrity == WEAR_LEVELING_INTEGRITY_SUSPECT || report->log_truncated;
#else
    return false;
#endif // WEAR_LEVELING_ENABLE
}

void sval_nvm_health_string(char *buf, size_t len) {
    (void)len;
#ifdef WEAR_LEVELING_ENABLE
    const wear_leveling_report_t *report = wear_leveling_report();

    // The checksums are 64-bit; split them so this does not depend on the
    // reduced printf supporting long long.
    uint32_t stored_hi   = (uint32_t)(report->checksum_stored >> 32);
    uint32_t stored_lo   = (uint32_t)(report->checksum_stored & 0xFFFFFFFF);
    uint32_t computed_hi = (uint32_t)(report->checksum_computed >> 32);
    uint32_t computed_lo = (uint32_t)(report->checksum_computed & 0xFFFFFFFF);

    const char *state;
    switch (report->integrity) {
        case WEAR_LEVELING_INTEGRITY_OK:
            state = "ok";
            break;
        case WEAR_LEVELING_INTEGRITY_BLANK:
            state = "blank (first boot after erase)";
            break;
        case WEAR_LEVELING_INTEGRITY_SUSPECT:
            state = report->contents_preserved ? "SUSPECT - contents preserved, NOT reset to defaults"
                                               : "SUSPECT - contents were zeroed";
            break;
        default:
            state = "unknown";
            break;
    }

    sprintf(buf, "NVM: %s%s | checksum stored %08lX%08lX computed %08lX%08lX | %lu log entries replayed\n",
            state, report->log_truncated ? " | WRITE LOG TRUNCATED" : "",
            (unsigned long)stored_hi, (unsigned long)stored_lo,
            (unsigned long)computed_hi, (unsigned long)computed_lo,
            (unsigned long)report->log_entries);
#else
    sprintf(buf, "NVM: wear leveling not enabled in this build\n");
#endif // WEAR_LEVELING_ENABLE
}

void sval_nvm_health_report(void) {
#ifdef CONSOLE_ENABLE
    // Note: QMK's debug dprintf() is not usable here -- stdio.h is needed for
    // sprintf() and brings in POSIX dprintf(int fd, ...), which shadows it.
    char buf[192];
    sval_nvm_health_string(buf, sizeof(buf));
    uprintf("%s", buf);
    if (sval_nvm_degraded()) {
        uprintf("NVM: stored configuration was kept rather than reset. Please report this,\n");
        uprintf("NVM: and dump flash before reflashing: picotool save -r 0x101E0000 0x10200000 dump.bin -f\n");
    }
#endif // CONSOLE_ENABLE
}
