// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"

#include "compiler_support.h"
#include "config_mirror.h"
#include "eeconfig.h"
#include "eeprom.h"
#include "timer.h"
#include "keyboard.h"

#include "via.h"
#include "nvm_eeconfig.h"
#include "quantum/nvm/eeprom/nvm_eeprom_eeconfig_internal.h"
#include "quantum/nvm/eeprom/nvm_eeprom_via_internal.h"

#ifdef WEAR_LEVELING_ENABLE
#    include "wear_leveling_rp2040_flash_config.h"
#endif

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

// How much of the logical EEPROM to mirror, starting at 0. This has to reach at
// least to the end of the dynamic keymap; the macro buffer that follows is the
// bulk of the store and is deliberately left out, because mirroring 63 KiB would
// mean erasing and rewriting it constantly for no reliability gain.
#ifndef SVAL_CONFIG_MIRROR_SIZE
#    define SVAL_CONFIG_MIRROR_SIZE 2048
#endif

#define SVAL_MIRROR_KEYMAP_END (VIA_EEPROM_CONFIG_END + ((DYNAMIC_KEYMAP_LAYER_COUNT) * (MATRIX_ROWS) * (MATRIX_COLS) * 2))

STATIC_ASSERT(SVAL_CONFIG_MIRROR_SIZE >= SVAL_MIRROR_KEYMAP_END, "SVAL_CONFIG_MIRROR_SIZE must cover the dynamic keymap; raise it");

#define SVAL_MIRROR_MAGIC 0x524D5653UL // "SVMR"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint32_t length;
    uint32_t hash;
} sval_mirror_header_t;

STATIC_ASSERT(sizeof(sval_mirror_header_t) == 16, "Unexpected mirror header size");
STATIC_ASSERT((sizeof(sval_mirror_header_t) + SVAL_CONFIG_MIRROR_SIZE) <= FLASH_SECTOR_SIZE, "Mirror slot does not fit in one flash sector");

// Two slots, written alternately, so the mirror is never the only copy while one
// of them is mid-erase. They sit immediately below the wear-leveling region, in
// flash that nothing else uses.
#ifndef SVAL_MIRROR_FLASH_BASE
#    define SVAL_MIRROR_FLASH_BASE ((WEAR_LEVELING_RP2040_FLASH_BASE) - (2 * (FLASH_SECTOR_SIZE)))
#endif

#define SVAL_MIRROR_SLOT_COUNT 2
#define SVAL_MIRROR_SLOT_OFFSET(slot) ((SVAL_MIRROR_FLASH_BASE) + ((slot) * (FLASH_SECTOR_SIZE)))
#define SVAL_MIRROR_SLOT_PTR(slot) ((const uint8_t *)((XIP_BASE) + SVAL_MIRROR_SLOT_OFFSET(slot)))

// The slots must sit below the EEPROM region and above the firmware. The lower
// bound is checked at link time by the firmware simply not reaching this far; the
// upper bound is checkable here and is the one that would silently corrupt config.
STATIC_ASSERT((SVAL_MIRROR_FLASH_BASE) % (FLASH_SECTOR_SIZE) == 0, "Mirror base must be sector aligned");
STATIC_ASSERT((SVAL_MIRROR_FLASH_BASE) + ((SVAL_MIRROR_SLOT_COUNT) * (FLASH_SECTOR_SIZE)) <= (WEAR_LEVELING_RP2040_FLASH_BASE), "Mirror slots overlap the EEPROM region");

// How long the configuration must sit unchanged, and the keyboard stay idle,
// before the mirror is rewritten. Writing costs an erase plus a program with
// interrupts disabled, so it must not happen while someone is typing.
#ifndef SVAL_MIRROR_IDLE_MS
#    define SVAL_MIRROR_IDLE_MS 5000
#endif
#ifndef SVAL_MIRROR_CHECK_INTERVAL_MS
#    define SVAL_MIRROR_CHECK_INTERVAL_MS 15000
#endif

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static bool     mirror_restored = false;
static bool     mirror_present  = false;
static uint32_t mirror_sequence = 0;
static uint32_t mirror_hash     = 0;
static uint8_t  mirror_slot     = 0; // slot holding the live mirror
static uint32_t last_check      = 0;

// FNV1a-32. Same family as the hash the wear-leveling layer uses, and small.
static uint32_t sval_mirror_hash(const uint8_t *data, uint32_t length) {
    uint32_t hash = 0x811C9DC5UL;
    for (uint32_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 0x01000193UL;
    }
    return hash;
}

static bool slot_is_valid(uint8_t slot, const sval_mirror_header_t **out) {
    const sval_mirror_header_t *header = (const sval_mirror_header_t *)SVAL_MIRROR_SLOT_PTR(slot);
    if (header->magic != SVAL_MIRROR_MAGIC) {
        return false;
    }
    if (header->length != SVAL_CONFIG_MIRROR_SIZE) {
        return false;
    }
    const uint8_t *payload = SVAL_MIRROR_SLOT_PTR(slot) + sizeof(sval_mirror_header_t);
    if (sval_mirror_hash(payload, header->length) != header->hash) {
        return false;
    }
    if (out) {
        *out = header;
    }
    return true;
}

// Picks the newest slot that passes its own hash check.
static bool find_live_slot(uint8_t *slot_out, const sval_mirror_header_t **header_out) {
    bool     found = false;
    uint8_t  best_slot = 0;
    uint32_t best_seq  = 0;

    for (uint8_t slot = 0; slot < SVAL_MIRROR_SLOT_COUNT; ++slot) {
        const sval_mirror_header_t *header = NULL;
        if (!slot_is_valid(slot, &header)) {
            continue;
        }
        if (!found || header->sequence > best_seq) {
            found     = true;
            best_slot = slot;
            best_seq  = header->sequence;
        }
    }

    if (found) {
        *slot_out = best_slot;
        if (header_out) {
            *header_out = (const sval_mirror_header_t *)SVAL_MIRROR_SLOT_PTR(best_slot);
        }
    }
    return found;
}

static void write_slot(uint8_t slot, uint32_t sequence, const uint8_t *payload) {
    static uint8_t page[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

    sval_mirror_header_t header = {
        .magic    = SVAL_MIRROR_MAGIC,
        .sequence = sequence,
        .length   = SVAL_CONFIG_MIRROR_SIZE,
        .hash     = sval_mirror_hash(payload, SVAL_CONFIG_MIRROR_SIZE),
    };

    memset(page, 0xFF, sizeof(page));
    memcpy(page, &header, sizeof(header));
    memcpy(page + sizeof(header), payload, SVAL_CONFIG_MIRROR_SIZE);

    // flash_range_erase()/flash_range_program() restore fast XIP via boot2 on the
    // way out, so this does not leave the chip in the ROM's slow read mode.
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(SVAL_MIRROR_SLOT_OFFSET(slot), FLASH_SECTOR_SIZE);
    flash_range_program(SVAL_MIRROR_SLOT_OFFSET(slot), page, sizeof(page));
    restore_interrupts(interrupts);
}

// ---------------------------------------------------------------------------
// Is the live configuration still there?
// ---------------------------------------------------------------------------

// The core signature is written once and never rewritten in normal operation, so
// its absence means the store was wiped or came back unreadable. Note this is
// deliberately not via_eeprom_is_valid(): that fails after every firmware rebuild
// because Vial derives its magic from a random BUILD_ID, and a firmware update is
// not a reason to restore.
static bool live_config_is_lost(void) {
    if (eeconfig_storage_is_suspect()) {
        return true;
    }
    uint16_t magic = eeprom_read_word(EECONFIG_MAGIC);
    return magic != EECONFIG_MAGIC_NUMBER;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void sval_config_mirror_init(void) {
    const sval_mirror_header_t *header = NULL;
    mirror_present = find_live_slot(&mirror_slot, &header);

    if (mirror_present) {
        mirror_sequence = header->sequence;
        mirror_hash     = header->hash;
    }

    if (!mirror_present || !live_config_is_lost()) {
        return;
    }

    // The primary is gone and we have a good copy. Put it back before anything
    // upstream notices the store looks uninitialised and "helpfully" resets it.
    const uint8_t *payload = SVAL_MIRROR_SLOT_PTR(mirror_slot) + sizeof(sval_mirror_header_t);
    eeprom_write_block(payload, (void *)0, SVAL_CONFIG_MIRROR_SIZE);
    mirror_restored = true;
}

void sval_config_mirror_task(void) {
    if (timer_elapsed32(last_check) < SVAL_MIRROR_CHECK_INTERVAL_MS) {
        return;
    }
    last_check = timer_read32();

    // Never copy a configuration we do not trust over a good mirror.
    if (live_config_is_lost()) {
        return;
    }

    // An erase plus a program runs with interrupts disabled; wait for a gap.
    if (last_input_activity_elapsed() < SVAL_MIRROR_IDLE_MS) {
        return;
    }

    static uint8_t live[SVAL_CONFIG_MIRROR_SIZE];
    eeprom_read_block(live, (const void *)0, SVAL_CONFIG_MIRROR_SIZE);

    uint32_t hash = sval_mirror_hash(live, SVAL_CONFIG_MIRROR_SIZE);
    if (mirror_present && hash == mirror_hash) {
        return; // already mirrored
    }

    uint8_t target = mirror_present ? (uint8_t)((mirror_slot + 1) % SVAL_MIRROR_SLOT_COUNT) : 0;
    write_slot(target, mirror_sequence + 1, live);

    // Only adopt the new slot if it reads back clean.
    if (slot_is_valid(target, NULL)) {
        mirror_slot     = target;
        mirror_sequence = mirror_sequence + 1;
        mirror_hash     = hash;
        mirror_present  = true;
    }
}

bool sval_config_mirror_restored(void) {
    return mirror_restored;
}

bool sval_config_mirror_present(void) {
    return mirror_present;
}

uint32_t sval_config_mirror_sequence(void) {
    return mirror_sequence;
}

