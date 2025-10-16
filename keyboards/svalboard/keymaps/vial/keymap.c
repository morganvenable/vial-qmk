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

layer_state_t default_layer_state_set_user(layer_state_t state) {
  sval_set_active_layer(0, false);
  return state;
}

layer_state_t layer_state_set_user(layer_state_t state) {
  sval_set_active_layer(get_highest_layer(state), false);
  return state;
}

enum layer {
    NORMAL,
    NAV,
    NAS,
    FUNC
    BOARD_CONFIG,
    MBO = MH_AUTO_BUTTONS_LAYER,
};

#if __has_include("keymap_all.h")
#include "keymap_all.h"
#else
int sval_macro_size = 0;
uint8_t sval_macros[] = {0};
const uint16_t PROGMEM keymaps[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    /* ===== LAYER_0 ===== */
    [0] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_J              , KC_U              , KC_QUOTE          , KC_M              , KC_H              , KC_LEFT           ,
        /*R2*/ KC_K              , KC_I              , LSFT(KC_SCOLON)   , KC_COMMA          , KC_Y              , KC_UP             ,
        /*R3*/ KC_L              , KC_O              , MO(4)             , KC_DOT            , KC_N              , KC_DOWN           ,
        /*R4*/ KC_SCOLON         , KC_P              , KC_BSLASH         , KC_SLASH          , KC_RBRACKET       , KC_RIGHT          ,
        /*L1*/ KC_F              , KC_R              , KC_G              , KC_V              , LSFT(KC_QUOTE)    , KC_END            ,
        /*L2*/ KC_D              , KC_E              , KC_T              , KC_C              , KC_GRAVE          , KC_PGDOWN         ,
        /*L3*/ KC_S              , KC_W              , KC_B              , KC_X              , KC_ESCAPE         , KC_PGUP           ,
        /*L4*/ KC_A              , KC_Q              , KC_LBRACKET       , KC_Z              , KC_DELETE         , KC_HOME           ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ MO(1)             , KC_SPACE          , MO(2)             , KC_BSPACE         , KC_LALT           , MO(3)             ,
        /*LT*/ KC_NO             , LT1(KC_ENTER)     , KC_LSHIFT         , LGUI_T(KC_TAB)    , KC_LCTRL          , KC_CAPSLOCK       ,
        ),

    /* ===== LAYER_1 ===== */
    [1] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_LEFT           , KC_NO             , KC_NO             , KC_HOME           , KC_NO             , KC_NO             ,
        /*R2*/ KC_UP             , KC_NO             , KC_NO             , KC_PGUP           , KC_NO             , KC_NO             ,
        /*R3*/ KC_DOWN           , KC_NO             , KC_NO             , KC_PGDOWN         , KC_INSERT         , KC_NO             ,
        /*R4*/ KC_RIGHT          , KC_NO             , KC_NO             , KC_END            , KC_NO             , KC_NO             ,
        /*L1*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L2*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L3*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        ),

    /* ===== LAYER_2 ===== */
    [2] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_7              , LSFT(KC_7)        , LSFT(KC_MINUS)    , KC_KP_PLUS        , KC_6              , KC_NO             ,
        /*R2*/ KC_8              , KC_KP_ASTERISK    , KC_TRNS           , KC_COMMA          , LSFT(KC_6)        , KC_NO             ,
        /*R3*/ KC_9              , LSFT(KC_9)        , KC_TRNS           , KC_DOT            , KC_TRNS           , KC_NO             ,
        /*R4*/ KC_0              , LSFT(KC_0)        , KC_BSLASH         , LSFT(KC_SLASH)    , KC_RBRACKET       , KC_NO             ,
        /*L1*/ KC_4              , LSFT(KC_4)        , KC_5              , KC_MINUS          , LSFT(KC_5)        , KC_NO             ,
        /*L2*/ KC_3              , LSFT(KC_3)        , KC_TRNS           , LSFT(KC_5)        , KC_TRNS           , KC_NO             ,
        /*L3*/ KC_2              , LSFT(KC_2)        , KC_TRNS           , KC_X              , KC_TRNS           , KC_NO             ,
        /*L4*/ KC_1              , LSFT(KC_1)        , LSFT(KC_GRAVE)    , KC_EQUAL          , KC_TRNS           , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        ),

    /* ===== LAYER_3 ===== */
    [3] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_F7             , KC_F16            , KC_NO             , KC_F17            , KC_F6             , KC_NO             ,
        /*R2*/ KC_F8             , KC_NO             , KC_NO             , KC_F18            , KC_NO             , KC_NO             ,
        /*R3*/ KC_F9             , KC_NO             , KC_NO             , KC_F19            , KC_NO             , KC_NO             ,
        /*R4*/ KC_F10            , KC_NO             , KC_NO             , KC_F20            , KC_NO             , KC_NO             ,
        /*L1*/ KC_F4             , KC_F24            , KC_F5             , KC_F14            , KC_F24            , KC_NO             ,
        /*L2*/ KC_F3             , KC_F23            , KC_NO             , KC_F13            , KC_NO             , KC_NO             ,
        /*L3*/ KC_F2             , KC_F22            , KC_NO             , KC_F12            , KC_NO             , KC_NO             ,
        /*L4*/ KC_F1             , KC_F21            , KC_NO             , KC_F11            , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        ),

    /* ===== LAYER_4 ===== */
    [4] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R2*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R3*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L1*/ KC_TRNS           , USER03            , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L2*/ KC_TRNS           , USER02            , KC_TRNS           , USER05            , KC_TRNS           , KC_NO             ,
        /*L3*/ KC_TRNS           , USER01            , KC_NO             , USER04            , USER09            , KC_NO             ,
        /*L4*/ USER17            , USER00            , KC_TRNS           , USER07            , KC_TRNS           , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        ),

    /* ===== LAYER_5 ===== */
    [5] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_LEFT           , KC_NO             , LSFT(KC_MINUS)    , KC_HOME           , KC_MINUS          , KC_NO             ,
        /*R2*/ KC_UP             , KC_NO             , KC_NO             , KC_PGUP           , LSFT(KC_6)        , KC_NO             ,
        /*R3*/ KC_DOWN           , LSFT(KC_9)        , KC_NO             , KC_PGDOWN         , KC_INSERT         , KC_NO             ,
        /*R4*/ KC_RIGHT          , LSFT(KC_0)        , KC_BSLASH         , KC_END            , KC_RBRACKET       , KC_NO             ,
        /*L1*/ KC_3              , KC_9              , KC_MINUS          , KC_6              , LSFT(KC_EQUAL)    , KC_NO             ,
        /*L2*/ KC_2              , KC_8              , KC_DOT            , KC_5              , KC_NO             , KC_NO             ,
        /*L3*/ KC_1              , KC_7              , KC_COMMA          , KC_4              , KC_NO             , KC_NO             ,
        /*L4*/ KC_0              , KC_NO             , KC_LBRACKET       , KC_EQUAL          , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        ),

    /* ===== LAYER_6 ===== */
    [6] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_7 ===== */
    [7] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_8 ===== */
    [8] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_9 ===== */
    [9] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_10 ===== */
    [10] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_11 ===== */
    [11] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_12 ===== */
    [12] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_TRNS           , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_13 ===== */
    [13] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_14 ===== */
    [14] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*R4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L1*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L2*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L3*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*L4*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        /*LT*/ KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             , KC_NO             ,
        ),

    /* ===== LAYER_15 ===== */
    [15] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R2*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R3*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L1*/ KC_BTN1           , KC_TRNS           , KC_TRNS           , LCTL(KC_V)        , KC_NO             , KC_NO             ,
        /*L2*/ KC_BTN3           , KC_TRNS           , KC_TRNS           , LCTL(KC_C)        , KC_NO             , KC_NO             ,
        /*L3*/ KC_BTN2           , KC_TRNS           , KC_TRNS           , TD(10)            , KC_TRNS           , KC_NO             ,
        /*L4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , USER13            , KC_TRNS           , KC_NO             ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_BTN1           , KC_TRNS           , KC_BTN2           , KC_TRNS           , KC_TRNS           ,
        ),
};
#endif

bool achordion_chord(uint16_t tap_hold_keycode, keyrecord_t* tap_hold_record,
                     uint16_t other_keycode, keyrecord_t* other_record) {
    if (tap_hold_record->event.key.row == 0 || tap_hold_record->event.key.row == 5 ||
        other_record->event.key.row    == 0 || other_record->event.key.row    == 5) {
        return true;
    }

    return achordion_opposite_hands(tap_hold_record, other_record);
}

void keyboard_post_init_user(void) {
  // Customise these values if you need to debug the matrix
  //debug_enable=true;
  //debug_matrix=true;
  //debug_keyboard=true;
  //debug_mouse=true;

#if __has_include("keymap_all.h")
  if (fresh_install) {
    sval_init_defaults();
  }
#endif
}
