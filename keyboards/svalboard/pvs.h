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
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Proportional Velocity Scrolling (PVS)
//
// Hold a key to set a trackball zero point. Displacement from zero
// controls scroll velocity. Return to center = stop scrolling.

// Configuration stored in EEPROM
typedef struct __attribute__((__packed__)) {
    uint8_t dead_zone_enter;     // Activation radius (normalized units, default 30)
    uint8_t dead_zone_exit;      // Deactivation radius for hysteresis (default 20)
    uint8_t max_displacement;    // Full travel range (default 250, stored as-is)
    uint8_t curve_mode;          // 0 = continuous quadratic, 1 = zoned/shuttle
    uint8_t max_velocity_index;  // Index into velocity table (0-7, default 4)
    uint8_t decay_speed;         // Reserved (was: position decay rate). Position no longer decays; kept for EEPROM layout compat.
    uint8_t gamma_index;         // 0=linear, 1=quadratic, 2=cubic (default 1)
    uint8_t reserved;            // Future use
} pvs_config_t;

// Default configuration values
#define PVS_DEFAULT_DEAD_ZONE_ENTER   0
#define PVS_DEFAULT_DEAD_ZONE_EXIT    0
#define PVS_DEFAULT_MAX_DISPLACEMENT  250
#define PVS_DEFAULT_CURVE_MODE        0
#define PVS_DEFAULT_MAX_VEL_INDEX     4
#define PVS_DEFAULT_DECAY_SPEED       128
#define PVS_DEFAULT_GAMMA_INDEX       1
#define PVS_DEFAULT_CONFIG { \
    PVS_DEFAULT_DEAD_ZONE_ENTER, \
    PVS_DEFAULT_DEAD_ZONE_EXIT, \
    PVS_DEFAULT_MAX_DISPLACEMENT, \
    PVS_DEFAULT_CURVE_MODE, \
    PVS_DEFAULT_MAX_VEL_INDEX, \
    PVS_DEFAULT_DECAY_SPEED, \
    PVS_DEFAULT_GAMMA_INDEX, \
    0 \
}

// Public API
void pvs_activate(uint16_t current_dpi);
void pvs_deactivate(void);
bool pvs_is_active(void);

// Called every pointing device task cycle (~1ms).
// Feeds trackball deltas into PVS position tracker.
// Outputs scroll values into out_h/out_v when the flush timer fires.
void pvs_process_deltas(int16_t dx, int16_t dy, int16_t *out_h, int16_t *out_v);

// Configuration
void pvs_set_config(const pvs_config_t *config);
void pvs_cycle_mode(void);
void pvs_speed_up(void);
void pvs_speed_down(void);
