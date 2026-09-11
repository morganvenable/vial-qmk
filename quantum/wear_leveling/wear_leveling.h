// Copyright 2022 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @typedef Status returned from any wear-leveling API.
 */
typedef enum wear_leveling_status_t {
    WEAR_LEVELING_FAILED,      //< Invocation failed
    WEAR_LEVELING_SUCCESS,     //< Invocation succeeded
    WEAR_LEVELING_CONSOLIDATED //< Invocation succeeded, consolidation occurred
} wear_leveling_status_t;

/**
 * @typedef Confidence in the contents of the backing store after initialization.
 *
 * The consolidated area is protected by a single FNV1a-64 over the whole logical
 * image, so any one bad byte invalidates all of it. Treating that as "the data is
 * invalid, overwrite it with defaults" turns a one-byte fault into total loss of
 * the user's configuration, which is strictly worse than carrying on with data
 * that is almost certainly still good. These states let callers tell the two
 * situations apart instead of conflating them.
 */
typedef enum wear_leveling_integrity_t {
    WEAR_LEVELING_INTEGRITY_OK,      //< Checksum matched; contents are trustworthy
    WEAR_LEVELING_INTEGRITY_BLANK,   //< Backing store is erased; there is nothing to lose
    WEAR_LEVELING_INTEGRITY_SUSPECT, //< Checksum failed but contents are present
} wear_leveling_integrity_t;

/**
 * @typedef Diagnostic record describing the most recent wear_leveling_init().
 */
typedef struct wear_leveling_report_t {
    wear_leveling_integrity_t integrity;
    bool                      checksum_ok;        //< Consolidated area matched its checksum
    bool                      contents_preserved; //< Suspect contents were kept rather than zeroed
    bool                      log_truncated;      //< An unusable write-log entry stopped playback
    uint32_t                  log_entries;        //< Write-log entries replayed successfully
    uint64_t                  checksum_stored;    //< Checksum read out of the backing store
    uint64_t                  checksum_computed;  //< Checksum computed over the consolidated area
} wear_leveling_report_t;

/**
 * Retrieves the diagnostic record from the most recent wear_leveling_init().
 *
 * Never returns NULL. Before the first init() the record reads as all-zero, which
 * reports as WEAR_LEVELING_INTEGRITY_OK.
 *
 * @return Pointer to the report
 */
const wear_leveling_report_t* wear_leveling_report(void);

/**
 * Wear-leveling initialization
 *
 * @return Status of the request
 */
wear_leveling_status_t wear_leveling_init(void);

/**
 * Wear-leveling erasure.
 *
 * Clears the wear-leveling area, with the definition that the "reset state" of all data is zero.
 *
 * @return Status of the request
 */
wear_leveling_status_t wear_leveling_erase(void);

/**
 * Writes logical data into the backing store.
 *
 * Skips writes if there are no changes to written values. The entire written block is considered when attempting to
 * determine if an overwrite should occur -- if there is any data mismatch the entire block will be written to the log,
 * not just the changed bytes.
 *
 * @param address[in] the logical address to write data
 * @param value[in] pointer to the source buffer
 * @param length[in] length of the data
 * @return Status of the request
 */
wear_leveling_status_t wear_leveling_write(uint32_t address, const void* value, size_t length);

/**
 * Reads logical data from the cache.
 *
 * @param address[in] the logical address to read data
 * @param value[out] pointer to the destination buffer
 * @param length[in] length of the data
 * @return Status of the request
 */
wear_leveling_status_t wear_leveling_read(uint32_t address, void* value, size_t length);
