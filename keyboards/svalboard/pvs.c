/*
Copyright 2025 Morgan Venable @_claussen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "pvs.h"
#include "timer.h"
#include <stdlib.h>

// ─── Internal types ────────────────────────────────────────────────
typedef enum {
    PVS_IDLE,
    PVS_DEADZONE,
    PVS_SCROLLING
} pvs_state_t;

// Q16.16 fixed-point position
typedef struct {
    int32_t x;
    int32_t y;
} pvs_pos_t;

// Fractional scroll accumulator (Q16.16)
typedef struct {
    int32_t x;
    int32_t y;
} pvs_accum_t;

// ─── Constants ─────────────────────────────────────────────────────
#define PVS_REFERENCE_DPI     800
#define PVS_FLUSH_MS          10    // Matches existing SCROLL_FREQUENCY_MS
#define PVS_POS_MAX_INT       500   // Max displacement in normalized units

// Max velocity table: hi-res scroll counts per second (120 counts = 1 detent)
// Index 0..7 gives progressively faster top speeds
static const int32_t pvs_max_vel_table[] = {
    360,     // 0: 3 detents/sec
    1200,    // 1: 10 detents/sec
    3600,    // 2: 30 detents/sec
    7200,    // 3: 60 detents/sec
    14400,   // 4: 120 detents/sec (default)
    21600,   // 5: 180 detents/sec
    36000,   // 6: 300 detents/sec
    60000,   // 7: 500 detents/sec
};
#define PVS_MAX_VEL_TABLE_SIZE (sizeof(pvs_max_vel_table) / sizeof(pvs_max_vel_table[0]))

// Zone velocities for shuttle emulation mode (7 zones)
static const int32_t pvs_zone_velocities[] = {
    120,     // Zone 1: 1 detent/sec
    360,     // Zone 2: 3 detents/sec
    1200,    // Zone 3: 10 detents/sec
    3600,    // Zone 4: 30 detents/sec
    7200,    // Zone 5: 60 detents/sec
    14400,   // Zone 6: 120 detents/sec
    28800,   // Zone 7: 240 detents/sec
};
#define PVS_NUM_ZONES (sizeof(pvs_zone_velocities) / sizeof(pvs_zone_velocities[0]))

// ─── State ─────────────────────────────────────────────────────────
static pvs_state_t  state = PVS_IDLE;
static pvs_pos_t    pos   = {0, 0};
static pvs_accum_t  accum = {0, 0};
static int32_t      dpi_scale = (1 << 16);  // Q16.16, default 1.0
static uint16_t     flush_timer = 0;
static pvs_config_t config = PVS_DEFAULT_CONFIG;

// ─── Helpers ───────────────────────────────────────────────────────

// Integer square root via Newton's method
static uint32_t isqrt32(uint32_t n) {
    if (n == 0) return 0;
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

static inline int32_t clamp32(int32_t val, int32_t lo, int32_t hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// Convert decay_speed byte (0-255) to decay ticks (500-10000)
static inline int32_t decay_ticks_from_config(void) {
    // Linear interpolation: 0 → 500, 255 → 10000
    return 500 + ((int32_t)config.decay_speed * 9500) / 255;
}

// Get max velocity from config index
static inline int32_t max_velocity(void) {
    uint8_t idx = config.max_velocity_index;
    if (idx >= PVS_MAX_VEL_TABLE_SIZE) idx = PVS_MAX_VEL_TABLE_SIZE - 1;
    return pvs_max_vel_table[idx];
}

// ─── Position tracking ────────────────────────────────────────────

static void pvs_accumulate_pos(int16_t dx, int16_t dy) {
    int32_t max_pos = (int32_t)PVS_POS_MAX_INT << 16;

    pos.x += (int32_t)dx * dpi_scale;
    pos.y += (int32_t)dy * dpi_scale;

    pos.x = clamp32(pos.x, -max_pos, max_pos);
    pos.y = clamp32(pos.y, -max_pos, max_pos);
}

static void pvs_apply_decay(void) {
    int32_t ticks = decay_ticks_from_config();

    int32_t decay_x = pos.x / ticks;
    int32_t decay_y = pos.y / ticks;

    // Anti-stall: ensure progress toward zero even for small positions
    if (pos.x != 0 && decay_x == 0) decay_x = (pos.x > 0) ? 1 : -1;
    if (pos.y != 0 && decay_y == 0) decay_y = (pos.y > 0) ? 1 : -1;

    pos.x -= decay_x;
    pos.y -= decay_y;
}

// ─── Dead zone with hysteresis ────────────────────────────────────

// Returns magnitude of position in integer units (from Q16.16)
static uint16_t pvs_magnitude(void) {
    int32_t ix = pos.x >> 16;
    int32_t iy = pos.y >> 16;
    uint32_t mag_sq = (uint32_t)((int32_t)ix * ix + (int32_t)iy * iy);
    return (uint16_t)isqrt32(mag_sq);
}

// Returns magnitude squared (for threshold comparison without sqrt)
static int32_t pvs_magnitude_sq(void) {
    int32_t ix = pos.x >> 16;
    int32_t iy = pos.y >> 16;
    return ix * ix + iy * iy;
}

static void pvs_update_state(void) {
    int32_t mag_sq = pvs_magnitude_sq();
    int32_t enter_sq = (int32_t)config.dead_zone_enter * config.dead_zone_enter;
    int32_t exit_sq  = (int32_t)config.dead_zone_exit  * config.dead_zone_exit;

    if (state == PVS_DEADZONE && mag_sq >= enter_sq) {
        state = PVS_SCROLLING;
    } else if (state == PVS_SCROLLING && mag_sq < exit_sq) {
        state = PVS_DEADZONE;
        // Clear fractional scroll on re-entry to dead zone
        accum.x = 0;
        accum.y = 0;
    }
}

// ─── Velocity mapping ─────────────────────────────────────────────

static int32_t pvs_velocity_continuous(uint16_t mag) {
    uint16_t dz = config.dead_zone_enter;
    uint16_t max_disp = config.max_displacement;

    if (mag <= dz || max_disp <= dz) return 0;

    int32_t d = mag - dz;
    int32_t range = max_disp - dz;
    int32_t max_vel = max_velocity();

    // Apply gamma curve:
    //   gamma 0 (linear):    v = max_vel * d / range
    //   gamma 1 (quadratic): v = max_vel * d^2 / range^2
    //   gamma 2 (cubic):     v = max_vel * d^3 / range^3
    // Use int64_t to avoid overflow
    int64_t v;
    switch (config.gamma_index) {
        case 0: // linear
            v = (int64_t)max_vel * d / range;
            break;
        case 1: // quadratic (default)
            v = (int64_t)max_vel * d * d / ((int64_t)range * range);
            break;
        case 2: // cubic
            v = (int64_t)max_vel * d * d / ((int64_t)range * range);
            v = v * d / range;
            break;
        default:
            v = (int64_t)max_vel * d * d / ((int64_t)range * range);
            break;
    }

    if (v > max_vel) v = max_vel;
    return (int32_t)v;
}

static int32_t pvs_velocity_zoned(uint16_t mag) {
    uint16_t dz = config.dead_zone_enter;
    uint16_t max_disp = config.max_displacement;

    if (mag <= dz || max_disp <= dz) return 0;

    int32_t d = mag - dz;
    int32_t range = max_disp - dz;

    // Map displacement to zone index
    int32_t zone = (d * PVS_NUM_ZONES) / range;
    if (zone >= (int32_t)PVS_NUM_ZONES) zone = PVS_NUM_ZONES - 1;

    return pvs_zone_velocities[zone];
}

// ─── Public API ───────────────────────────────────────────────────

void pvs_activate(uint16_t current_dpi) {
    state = PVS_DEADZONE;
    pos.x = 0;
    pos.y = 0;
    accum.x = 0;
    accum.y = 0;
    if (current_dpi == 0) current_dpi = PVS_REFERENCE_DPI;
    dpi_scale = ((int32_t)PVS_REFERENCE_DPI << 16) / current_dpi;
    flush_timer = timer_read();
}

void pvs_deactivate(void) {
    state = PVS_IDLE;
    pos.x = 0;
    pos.y = 0;
    accum.x = 0;
    accum.y = 0;
}

bool pvs_is_active(void) {
    return state != PVS_IDLE;
}

void pvs_process_deltas(int16_t dx, int16_t dy, int16_t *out_h, int16_t *out_v) {
    *out_h = 0;
    *out_v = 0;

    if (state == PVS_IDLE) return;

    // 1. Accumulate DPI-normalized position
    pvs_accumulate_pos(dx, dy);

    // 2. Apply decay toward zero (counteracts sensor drift)
    pvs_apply_decay();

    // 3. Update state machine (dead zone hysteresis)
    pvs_update_state();

    // 4. Generate scroll output at flush interval
    if (state == PVS_SCROLLING && timer_elapsed(flush_timer) >= PVS_FLUSH_MS) {
        flush_timer = timer_read();

        uint16_t mag = pvs_magnitude();
        int32_t vel_mag;

        if (config.curve_mode == 0) {
            vel_mag = pvs_velocity_continuous(mag);
        } else {
            vel_mag = pvs_velocity_zoned(mag);
        }

        // Decompose velocity along position direction
        int32_t ix = pos.x >> 16;
        int32_t iy = pos.y >> 16;
        int32_t vel_x = 0, vel_y = 0;
        if (mag > 0) {
            vel_x = (int64_t)vel_mag * ix / mag;
            vel_y = (int64_t)vel_mag * iy / mag;
        }

        // Convert velocity (counts/sec) to counts per flush interval
        // vel * PVS_FLUSH_MS / 1000, in Q16.16 for fractional accumulation
        accum.x += ((int64_t)vel_x << 16) / 100;  // 10ms/1000ms = 1/100
        accum.y += ((int64_t)vel_y << 16) / 100;

        // Extract integer part for HID report
        *out_h = (int16_t)(accum.x >> 16);
        *out_v = (int16_t)(accum.y >> 16);

        // Keep fractional remainder
        accum.x -= ((int32_t)*out_h << 16);
        accum.y -= ((int32_t)*out_v << 16);
    } else if (state == PVS_DEADZONE) {
        // Reset flush timer while in dead zone so first scroll is immediate
        flush_timer = timer_read();
    }
}

void pvs_set_config(const pvs_config_t *new_config) {
    config = *new_config;
}

void pvs_cycle_mode(void) {
    config.curve_mode = (config.curve_mode + 1) % 2;
}

void pvs_speed_up(void) {
    if (config.max_velocity_index < PVS_MAX_VEL_TABLE_SIZE - 1) {
        config.max_velocity_index++;
    }
}

void pvs_speed_down(void) {
    if (config.max_velocity_index > 0) {
        config.max_velocity_index--;
    }
}
