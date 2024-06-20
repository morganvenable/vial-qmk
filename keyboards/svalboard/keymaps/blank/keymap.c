/*
Copyright 2023 Morgan Venable @_claussen

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

#include "../keymap_support.c"
#include "keycodes.h"
#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>
#include "svalboard.h"

#define LAYER_COLOR(name, color) rgblight_segment_t const (name)[] = RGBLIGHT_LAYER_SEGMENTS({0, 2, color})

LAYER_COLOR(layer0_colors, HSV_GREEN); // NORMAL
LAYER_COLOR(layer1_colors, HSV_GREEN); // NORMAL_HOLD
LAYER_COLOR(layer2_colors, HSV_ORANGE); // FUNC
LAYER_COLOR(layer3_colors, HSV_ORANGE); // FUNC_HOLD
LAYER_COLOR(layer4_colors, HSV_AZURE); // NAS
LAYER_COLOR(layer5_colors, HSV_AZURE); // would be NAS hold
LAYER_COLOR(layer6_colors, HSV_RED); // maybe 10kp
LAYER_COLOR(layer7_colors, HSV_RED);
LAYER_COLOR(layer8_colors, HSV_PINK);
LAYER_COLOR(layer9_colors, HSV_PURPLE);
LAYER_COLOR(layer10_colors, HSV_CORAL);
LAYER_COLOR(layer11_colors, HSV_SPRINGGREEN);
LAYER_COLOR(layer12_colors, HSV_TEAL);
LAYER_COLOR(layer13_colors, HSV_TURQUOISE);
LAYER_COLOR(layer14_colors, HSV_YELLOW);
LAYER_COLOR(layer15_colors, HSV_MAGENTA); // MBO
#undef LAYER_COLOR

const rgblight_segment_t*  const __attribute((weak))sval_rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    layer0_colors, layer1_colors, layer2_colors, layer3_colors,
    layer4_colors, layer5_colors, layer6_colors, layer7_colors,
    layer8_colors, layer9_colors, layer10_colors, layer11_colors,
    layer12_colors, layer13_colors, layer14_colors, layer15_colors
);

layer_state_t default_layer_state_set_user(layer_state_t state) {
  rgblight_set_layer_state(0, layer_state_cmp(state, 0));
  return state;
}

layer_state_t layer_state_set_user(layer_state_t state) {
  for (int i = 0; i < RGBLIGHT_LAYERS; ++i) {
      rgblight_set_layer_state(i, layer_state_cmp(state, i));
  }
  return state;
}



void keyboard_post_init_user(void) {
  // Customise these values if you need to debug the matrix
  //debug_enable=true;
  //debug_matrix=true;
  //debug_keyboard=true;
  //debug_mouse=true;
  rgblight_layers = sval_rgb_layers;
}

enum layer {
    NORMAL,
    NORMAL_HOLD,
    FUNC,
    FUNC_HOLD,
    NAS,
    MBO = MH_AUTO_BUTTONS_LAYER
};

const uint16_t PROGMEM keymaps[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    [NORMAL] = LAYOUT(
        /*Center           North           East            South           West*/

        /*R1*/ KC_NO,            KC_NO,           KC_NO,       KC_NO,           KC_NO, XXXXXXX,
        /*R2*/ KC_NO,            KC_NO,           KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*R3*/ KC_NO,            KC_NO,           KC_NO,        KC_NO,         KC_NO, XXXXXXX,
        /*R4*/ KC_NO,    KC_NO,           KC_NO,        KC_NO,       KC_NO, XXXXXXX,

        /*L1*/ KC_NO,            KC_NO,           KC_NO,           KC_NO,           KC_NO, XXXXXXX,
        /*L2*/ KC_NO,            KC_NO,           KC_NO,           KC_NO,           KC_NO, XXXXXXX,
        /*L3*/ KC_NO,            KC_NO,           KC_NO,           KC_NO,           KC_NO, XXXXXXX,
        /*L4*/ KC_NO,            KC_NO,           KC_NO,        KC_NO,           KC_NO, XXXXXXX,

        /*Down                  Inner (pad)     Upper (Mode)    O.Upper (nail)  OL (knuckle) Pushthrough*/
        /*RT*/ MO(NAS),         KC_NO,       TO(FUNC),       KC_NO,        KC_NO,     TG(NAS),
        /*LT*/ KC_NO,         KC_NO,       TO(NORMAL),          KC_NO,         KC_NO,     KC_NO
        ),

    [NORMAL_HOLD] = LAYOUT(
        /*Center           North           East            South           West*/
        /*R1*/ KC_NO,         KC_NO,        XXXXXXX,        KC_NO,        LCTL(KC_NO),  XXXXXXX,
        /*R2*/ KC_NO,         KC_NO,        XXXXXXX,        KC_NO,        LCTL(KC_NO),XXXXXXX,
        /*R3*/ KC_NO,           KC_NO,        XXXXXXX,        KC_NO,        LCTL(KC_NO),XXXXXXX,
        /*R4*/ KC_NO,        KC_NO,        XXXXXXX,        KC_NO,        LCTL(KC_NO),XXXXXXX,

        /*L1*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        KC_NO,        XXXXXXX,XXXXXXX,
        /*L2*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        KC_NO,        XXXXXXX,XXXXXXX,
        /*L3*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        KC_NO,        XXXXXXX,XXXXXXX,
        /*L4*/ DF(NORMAL),      _______,        _______,        XXXXXXX,       _______,XXXXXXX,

        /*Down                  Inner           Upper           Outer Upper     Outer Lower  Pushthrough*/
        /*RT*/ _______,         _______,        _______,        _______,        _______, _______,
        /*LT*/ _______,         _______,        _______,        _______,        _______, _______
        ),

    [FUNC] = LAYOUT(
        /*Center           North           East            South           West*/
        /*R1*/ KC_NO,         KC_NO,          KC_NO,       KC_NO,        KC_NO,XXXXXXX,
        /*R2*/ XXXXXXX,         KC_NO,          XXXXXXX,        KC_NO,          KC_NO,XXXXXXX,
        /*R3*/ KC_NO,         KC_NO,         KC_NO,        KC_NO,          KC_NO,XXXXXXX,
        /*R4*/ KC_NO,        KC_NO,        KC_NO,         KC_NO,        KC_NO,XXXXXXX,

        /*L1*/ KC_NO,         KC_NO,          KC_NO,       KC_NO,        KC_NO,XXXXXXX,
        /*L2*/ XXXXXXX,         KC_NO,          XXXXXXX,        KC_NO,          XXXXXXX,XXXXXXX,
        /*L3*/ XXXXXXX,         KC_NO,          XXXXXXX,        KC_NO,          KC_NO, XXXXXXX,
        /*L4*/ XXXXXXX,         KC_NO,          XXXXXXX,        KC_NO,          KC_NO,XXXXXXX,

        /*Down                  Inner           Upper           Outer Upper     Outer Lower  Pushthrough*/
        /*RT*/ MO(NAS),         KC_NO,       _______,       KC_NO,      KC_NO, _______,
        /*LT*/ KC_NO,       KC_NO,         _______, KC_NO,         KC_NO, XXXXXXX
        ),

    [FUNC_HOLD] = LAYOUT(
        /*Center           North           East            South           West*/
        /*R1*/ KC_NO,         LCTL(KC_NO),    LCTL(KC_NO), LCTL(KC_NO),  LCTL(KC_NO), XXXXXXX,
        /*R2*/ KC_NO,           KC_NO,        KC_NO,        KC_NO,        KC_NO, XXXXXXX,
        /*R3*/ KC_NO,         KC_NO,        KC_NO,        KC_NO,        KC_NO, XXXXXXX,
        /*R4*/ KC_NO,        XXXXXXX,        XXXXXXX,        XXXXXXX,        XXXXXXX,XXXXXXX,

        /*L1*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        XXXXXXX,     XXXXXXX,XXXXXXX,
        /*L2*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        XXXXXXX,     XXXXXXX,XXXXXXX,
        /*L3*/ XXXXXXX,         XXXXXXX,        XXXXXXX,        XXXXXXX,     XXXXXXX,XXXXXXX,
        /*L4*/ _______,      _______,        _______,        _______,       _______,XXXXXXX,

        /*Down                  Inner           Upper           Outer Upper     Outer Lower  Pushthrough*/
        /*RT*/ _______,         _______,        _______,        _______,        _______,_______,
        /*LT*/ _______,         _______,        _______,        _______,        _______, _______
        ),

    [NAS] = LAYOUT(
        /*Center           North           East            South           West*/
        /*R1*/ KC_NO,            KC_NO,        KC_NO,        KC_NO,     KC_NO, XXXXXXX,
        /*R2*/ KC_NO,            KC_NO, KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*R3*/ KC_NO,            KC_NO,        KC_NO,        KC_NO,         KC_NO, XXXXXXX,
        /*R4*/ KC_NO,            KC_NO,        XXXXXXX,        KC_NO,        KC_NO, XXXXXXX,

        /*L1*/ KC_NO,            KC_NO,      KC_NO,           KC_NO,       KC_NO, XXXXXXX,
        /*L2*/ KC_NO,            KC_NO,        KC_NO,          KC_NO,     KC_NO, XXXXXXX,
        /*L3*/ KC_NO,            KC_NO,          XXXXXXX,        KC_NO,           KC_NO, XXXXXXX,
        /*L4*/ KC_NO,            KC_NO,     KC_NO,       KC_NO,       KC_NO, XXXXXXX,

        /*Down                  Inner           Upper           Outer Upper     Outer Lower  Pushthrough*/
        /*RT*/ MO(NAS),         KC_NO,       _______,       KC_NO,        KC_NO, _______,
        /*LT*/ KC_NO,         KC_NO,       _______,        KC_NO,         KC_NO, _______
        ),

    [MBO] = LAYOUT(
        /*Center           North           East            South           West*/
        /*R1*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*R2*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*R3*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*R4*/ SV_RECALIBRATE_POINTER,        KC_NO,       KC_NO,       KC_NO,       KC_NO, XXXXXXX,
        /*L1*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,        KC_NO, XXXXXXX,
        /*L2*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,        KC_NO, XXXXXXX,
        /*L3*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,        KC_NO, XXXXXXX,
        /*L4*/ SV_RECALIBRATE_POINTER,        KC_NO,       KC_NO,       KC_NO,       KC_NO,XXXXXXX,
        /*RT*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,       KC_NO,   KC_NO,
        /*LT*/ KC_NO,        KC_NO,       KC_NO,       KC_NO,       KC_NO,   KC_NO
        )

};

bool achordion_chord(uint16_t tap_hold_keycode, keyrecord_t* tap_hold_record,
                     uint16_t other_keycode, keyrecord_t* other_record) {
    if (tap_hold_record->event.key.row == 0 || tap_hold_record->event.key.row == 5 ||
        other_record->event.key.row    == 0 || other_record->event.key.row    == 5) {
        return true;
    }

    return achordion_opposite_hands(tap_hold_record, other_record);
}

