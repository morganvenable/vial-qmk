// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Host-side test for the wear-leveling integrity behaviour.
//
// Compiles the real quantum/wear_leveling/wear_leveling.c against an in-memory
// backing store and asserts what happens to the user's data when the consolidated
// checksum does not match. See util/wear_leveling_test/run.sh.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fnv.h"
#include "wear_leveling.h"
#include "wear_leveling_internal.h"

// ---------------------------------------------------------------------------
// Fake backing store. Mirrors the RP2040 driver: values are stored inverted, so
// erased storage (0xFF) reads back as 0x0000.
// ---------------------------------------------------------------------------

static uint8_t storage[(WEAR_LEVELING_BACKING_SIZE)];

bool backing_store_init(void) {
    return true;
}
bool backing_store_unlock(void) {
    return true;
}
bool backing_store_lock(void) {
    return true;
}

bool backing_store_erase(void) {
    memset(storage, 0xFF, sizeof(storage));
    return true;
}

bool backing_store_write_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    for (size_t i = 0; i < item_count; ++i) {
        backing_store_int_t inverted = (backing_store_int_t)~values[i];
        memcpy(&storage[address + i * sizeof(backing_store_int_t)], &inverted, sizeof(inverted));
    }
    return true;
}

bool backing_store_write(uint32_t address, backing_store_int_t value) {
    return backing_store_write_bulk(address, &value, 1);
}

bool backing_store_read_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    for (size_t i = 0; i < item_count; ++i) {
        backing_store_int_t raw;
        memcpy(&raw, &storage[address + i * sizeof(backing_store_int_t)], sizeof(raw));
        values[i] = (backing_store_int_t)~raw;
    }
    return true;
}

bool backing_store_read(uint32_t address, backing_store_int_t *value) {
    return backing_store_read_bulk(address, value, 1);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#define VIA_MAGIC_ADDR 91
#define KEYMAP_ADDR 95
#define KEYMAP_SIZE (16 * 10 * 6 * 2)

static void store_put(uint32_t address, const void *data, size_t length) {
    const uint8_t *p = data;
    for (size_t i = 0; i < length; ++i) {
        storage[address + i] = (uint8_t)~p[i];
    }
}

// Lay down a consolidated image with a valid checksum, as a healthy board would have.
static void build_healthy_store(uint8_t *expected) {
    memset(storage, 0xFF, sizeof(storage));
    memset(expected, 0, (WEAR_LEVELING_LOGICAL_SIZE));

    uint16_t eeconfig_magic = 0xFEE6;
    memcpy(&expected[0], &eeconfig_magic, sizeof(eeconfig_magic));
    expected[VIA_MAGIC_ADDR + 0] = 0xA1;
    expected[VIA_MAGIC_ADDR + 1] = 0xB2;
    expected[VIA_MAGIC_ADDR + 2] = 0xC3;
    for (int i = 0; i < KEYMAP_SIZE; ++i) {
        expected[KEYMAP_ADDR + i] = (uint8_t)(0x40 + (i % 200));
    }

    store_put(0, expected, (WEAR_LEVELING_LOGICAL_SIZE));
    uint64_t checksum = fnv_64a_buf(expected, (WEAR_LEVELING_LOGICAL_SIZE), FNV1A_64_INIT);
    store_put((WEAR_LEVELING_LOGICAL_SIZE), &checksum, sizeof(checksum));
}

static int failures = 0;

#define CHECK(cond, msg)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            printf("  FAIL: %s\n", (msg));                \
            failures++;                                   \
        } else {                                          \
            printf("  ok:   %s\n", (msg));                \
        }                                                 \
    } while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_healthy_store(void) {
    printf("healthy store\n");
    uint8_t expected[(WEAR_LEVELING_LOGICAL_SIZE)];
    build_healthy_store(expected);

    wear_leveling_init();
    const wear_leveling_report_t *report = wear_leveling_report();

    CHECK(report->integrity == WEAR_LEVELING_INTEGRITY_OK, "integrity reported OK");
    CHECK(report->checksum_ok, "checksum matched");

    uint8_t readback[KEYMAP_SIZE];
    wear_leveling_read(KEYMAP_ADDR, readback, sizeof(readback));
    CHECK(memcmp(readback, &expected[KEYMAP_ADDR], KEYMAP_SIZE) == 0, "keymap read back intact");
}

static void test_blank_store(void) {
    printf("erased store (genuine first boot)\n");
    memset(storage, 0xFF, sizeof(storage));

    wear_leveling_init();
    const wear_leveling_report_t *report = wear_leveling_report();

    CHECK(report->integrity == WEAR_LEVELING_INTEGRITY_BLANK, "integrity reported BLANK");
    CHECK(!report->checksum_ok, "checksum did not match (expected on erased storage)");
    CHECK(!report->contents_preserved, "nothing claimed as preserved");
}

static void test_single_bit_corruption(void) {
    printf("one flipped bit in unused space\n");
    uint8_t expected[(WEAR_LEVELING_LOGICAL_SIZE)];
    build_healthy_store(expected);

    // Flip one bit a long way from anything the firmware actually reads.
    storage[50000] ^= 0x01;

    wear_leveling_init();
    const wear_leveling_report_t *report = wear_leveling_report();

    CHECK(!report->checksum_ok, "checksum mismatch detected");
    CHECK(report->integrity == WEAR_LEVELING_INTEGRITY_SUSPECT, "integrity reported SUSPECT");
    CHECK(report->contents_preserved, "contents preserved rather than zeroed");

    // This is the whole point: before the fix, every one of these reads returned 0,
    // the VIA magic check failed, and the keymap was replaced with the defaults.
    uint8_t magic[3];
    wear_leveling_read(VIA_MAGIC_ADDR, magic, sizeof(magic));
    CHECK(magic[0] == 0xA1 && magic[1] == 0xB2 && magic[2] == 0xC3, "VIA magic survived");

    uint8_t readback[KEYMAP_SIZE];
    wear_leveling_read(KEYMAP_ADDR, readback, sizeof(readback));
    CHECK(memcmp(readback, &expected[KEYMAP_ADDR], KEYMAP_SIZE) == 0, "user keymap survived");
}

static void test_log_replay_still_applies(void) {
    printf("suspect store still replays its write log\n");
    uint8_t expected[(WEAR_LEVELING_LOGICAL_SIZE)];
    build_healthy_store(expected);
    storage[50000] ^= 0x01;

    // Append a write the way the firmware would: a multibyte log entry.
    uint8_t edit[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
    write_log_entry_t log = LOG_ENTRY_MAKE_MULTIBYTE(KEYMAP_ADDR + 8, 5);
    memcpy(&log.raw8[3], edit, sizeof(edit));
    uint32_t at = (WEAR_LEVELING_LOGICAL_SIZE) + 8;
    for (int i = 0; i < 4; ++i) {
        store_put(at + i * 2, &log.raw16[i], 2);
    }

    wear_leveling_init();
    const wear_leveling_report_t *report = wear_leveling_report();

    CHECK(report->log_entries == 1, "one log entry replayed");
    CHECK(!report->log_truncated, "log not truncated");

    uint8_t readback[5];
    wear_leveling_read(KEYMAP_ADDR + 8, readback, sizeof(readback));
    CHECK(memcmp(readback, edit, sizeof(edit)) == 0, "logged edit applied on top of preserved data");
}

int main(void) {
    test_healthy_store();
    test_blank_store();
    test_single_bit_corruption();
    test_log_replay_still_applies();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
