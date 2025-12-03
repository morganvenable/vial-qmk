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
    NAVNAS,
    FUNC,
    BOARD_CONFIG = MH_AUTO_BUTTONS_LAYER - 1,
    MBO = MH_AUTO_BUTTONS_LAYER,
};

#if __has_include("keymap_all.h")
#include "keymap_all.h"
#else
int sval_macro_size = 0;
uint8_t sval_macros[] = {0};
const uint16_t PROGMEM keymaps[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    /* ===== NORMAL ===== .*/
    [NORMAL] = LAYOUT(
        /*     Center            North               East                South             West                Double*/
        /*R1*/ KC_J              , KC_U              , KC_QUOTE          , KC_M            , KC_H              , KC_NO           ,
        /*R2*/ KC_K              , KC_I              , KC_COLON          , KC_COMMA        , KC_Y              , KC_NO             ,
        /*R3*/ KC_L              , KC_O              , LT(BOARD_CONFIG, KC_NO)  , KC_DOT   , KC_N              , KC_NO           ,
        /*R4*/ KC_SEMICOLON      , KC_P              , KC_BSLS           , KC_SLASH        , KC_RBRC           , KC_NO          ,
        /*L1*/ KC_F              , KC_R              , KC_G              , KC_V            , LSFT(KC_QUOTE)    , KC_NO            ,
        /*L2*/ KC_D              , KC_E              , KC_T              , KC_C            , KC_GRAVE          , KC_NO         ,
        /*L3*/ KC_S              , KC_W              , KC_B              , KC_X            , KC_ESCAPE         , KC_NO           ,
        /*L4*/ KC_A              , KC_Q              , KC_LBRC           , KC_Z            , KC_DELETE         , KC_NO           ,
        
        /*     Down                 Pad                Up                  Nail            Knuckle             DoubleDown*/
        /*RT*/ MO(NAVNAS)          , KC_SPACE        , KC_NO             , KC_BSPC         , KC_LALT           , MO(FUNC)             ,
        /*LT*/ KC_LSFT       , LT(NAVNAS, KC_ENTER)  , KC_NO             , LGUI_T(KC_TAB)  , KC_LCTL           , KC_CAPS       
        ),

    [NAVNAS] = LAYOUT(
        /*     Center            North               East               South             West              Double*/
        /*R1*/ KC_7              , LSFT(KC_7)        , LSFT(KC_6)       , KC_LEFT         , KC_6            , KC_NO           ,
        /*R2*/ KC_8              , LSFT(KC_8)        , KC_NO            , KC_UP           , LSFT(KC_MINUS)  , KC_NO           ,
        /*R3*/ KC_9              , LSFT(KC_9)        , KC_NO            , KC_DOWN         , KC_INSERT       , KC_NO           ,
        /*R4*/ KC_0              , LSFT(KC_0)        , KC_NO            , KC_RIGHT        , LSFT(KC_GRAVE)  , KC_NO           ,
        /*L1*/ KC_4              , LSFT(KC_4)        , KC_5             , KC_END          , LSFT(KC_5)      , KC_NO           ,
        /*L2*/ KC_3              , LSFT(KC_3)        , KC_MINUS         , KC_PGDN       , LSFT(KC_EQUAL)  , KC_NO           ,
        /*L3*/ KC_2              , LSFT(KC_2)        , KC_DOT           , KC_PGUP         , KC_TRNS         , KC_NO           ,
        /*L4*/ KC_1              , LSFT(KC_1)        , KC_EQUAL         , KC_HOME         , KC_TRNS         , KC_NO           ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS          , KC_TRNS         , KC_TRNS         , KC_TRNS         ,
        /*LT*/ KC_TRNS           , KC_TRNS           , KC_TRNS          , KC_TRNS         , KC_TRNS         , KC_TRNS         
        ),
    
    /* ===== FUNCTION KEYS ===== */
    [FUNC] = LAYOUT(
        /*Center           North           East            South           West            Double*/
        /*R1*/ KC_F7            , KC_NO             , KC_F16           , KC_F17         , KC_F6           , KC_NO           ,
        /*R2*/ KC_F8            , KC_NO             , KC_NO            , KC_F18         , KC_NO           , KC_NO           ,
        /*R3*/ KC_F9            , KC_NO             , KC_NO            , KC_F19         , KC_NO           , KC_NO           ,
        /*R4*/ KC_F10           , KC_NO             , KC_NO            , KC_F20         , KC_NO           , KC_NO           ,
        /*L1*/ KC_F4            , KC_F24            , KC_F5            , KC_F14         , KC_F15          , KC_NO           ,
        /*L2*/ KC_F3            , KC_F23            , KC_F10           , KC_F13         , KC_NO           , KC_NO           ,
        /*L3*/ KC_F2            , KC_F22            , KC_NO            , KC_F12         , KC_NO           , KC_NO           ,
        /*L4*/ KC_F1            , KC_F21            , KC_NO            , KC_F11         , KC_NO           , KC_NO           ,
        /*Down             Pad             Up              Nail            Knuckle         DoubleDown*/
        /*RT*/ KC_TRNS          , KC_TRNS           , KC_TRNS          , KC_TRNS        , KC_TRNS         , KC_TRNS         ,
        /*LT*/ KC_TRNS          , KC_TRNS           , KC_TRNS          , KC_TRNS        , KC_TRNS         , KC_TRNS         
        ),

    [BOARD_CONFIG] = LAYOUT(
        /*         Center              North               East                South               West                (XXX)               */
        /*R1*/     KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_NO,
        /*R2*/     KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_NO,
        /*R3*/     KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_NO,
        /*R4*/     KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_NO,
        /*L1*/     SV_OUTPUT_STATUS,   KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_NO,
        /*L2*/     KC_TRNS,            SV_RIGHT_DPI_INC,   KC_TRNS,            SV_RIGHT_DPI_DEC,   KC_TRNS,            KC_NO,
        /*L3*/     KC_TRNS,            SV_LEFT_DPI_INC,    KC_TRNS,            SV_LEFT_DPI_DEC,    KC_TRNS,            KC_NO,
        /*L4*/     KC_TRNS,            KC_TRNS, KC_TRNS,   KC_TRNS,            KC_TRNS,            KC_NO,

        /*        Down                Pad                 Up                  Nail                Knuckle             Double Down         */
        /* RT */  KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,
        /* LT */  KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS,            KC_TRNS
      ),

    /* ===== MBO ===== */
    [MBO] = LAYOUT(
        /*      Center           North               East                South                West                Double*/
        /*R1*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R2*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R3*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*R4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L1*/ KC_BTN1           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L2*/ KC_BTN3           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L3*/ KC_BTN2           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_NO             ,
        /*L4*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , SV_SNIPER_3       , KC_TRNS           , KC_NO             ,
        
        /*     Down               Pad                Up                  Nail                Knuckle             DoubleDown */
        /*RT*/ KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           , KC_TRNS           ,
        /*LT*/ KC_TRNS           , KC_BTN1           , KC_TRNS           , KC_BTN2           , KC_TRNS           , KC_TRNS          
        ),
};
#endif

// Key override definitions - only active on NAVNAS layer
// Numbers (1-0) + Shift = F1-F10
const key_override_t ko_1_f1 = ko_make_with_layers(MOD_MASK_SHIFT, KC_1, KC_F1, 1 << NAVNAS);
const key_override_t ko_2_f2 = ko_make_with_layers(MOD_MASK_SHIFT, KC_2, KC_F2, 1 << NAVNAS);
const key_override_t ko_3_f3 = ko_make_with_layers(MOD_MASK_SHIFT, KC_3, KC_F3, 1 << NAVNAS);
const key_override_t ko_4_f4 = ko_make_with_layers(MOD_MASK_SHIFT, KC_4, KC_F4, 1 << NAVNAS);
const key_override_t ko_5_f5 = ko_make_with_layers(MOD_MASK_SHIFT, KC_5, KC_F5, 1 << NAVNAS);
const key_override_t ko_6_f6 = ko_make_with_layers(MOD_MASK_SHIFT, KC_6, KC_F6, 1 << NAVNAS);
const key_override_t ko_7_f7 = ko_make_with_layers(MOD_MASK_SHIFT, KC_7, KC_F7, 1 << NAVNAS);
const key_override_t ko_8_f8 = ko_make_with_layers(MOD_MASK_SHIFT, KC_8, KC_F8, 1 << NAVNAS);
const key_override_t ko_9_f9 = ko_make_with_layers(MOD_MASK_SHIFT, KC_9, KC_F9, 1 << NAVNAS);
const key_override_t ko_0_f10 = ko_make_with_layers(MOD_MASK_SHIFT, KC_0, KC_F10, 1 << NAVNAS);

// Symbols (shift+number keys) + Shift = F11-F20
// On QWERTY: ! @ # $ % ^ & * ( )
const key_override_t ko_exlm_f11 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_1), KC_F11, 1 << NAVNAS);  // !
const key_override_t ko_at_f12 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_2), KC_F12, 1 << NAVNAS);    // @
const key_override_t ko_hash_f13 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_3), KC_F13, 1 << NAVNAS);  // #
const key_override_t ko_dlr_f14 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_4), KC_F14, 1 << NAVNAS);   // $
const key_override_t ko_perc_f15 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_5), KC_F15, 1 << NAVNAS);  // %
const key_override_t ko_circ_f16 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_6), KC_F16, 1 << NAVNAS);  // ^
const key_override_t ko_ampr_f17 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_7), KC_F17, 1 << NAVNAS);  // &
const key_override_t ko_astr_f18 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_8), KC_F18, 1 << NAVNAS);  // *
const key_override_t ko_lprn_f19 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_9), KC_F19, 1 << NAVNAS);  // (
const key_override_t ko_rprn_f20 = ko_make_with_layers(MOD_MASK_SHIFT, LSFT(KC_0), KC_F20, 1 << NAVNAS);  // )

// Array of all key overrides
const key_override_t **key_overrides = (const key_override_t *[]){
    &ko_1_f1,
    &ko_2_f2,
    &ko_3_f3,
    &ko_4_f4,
    &ko_5_f5,
    &ko_6_f6,
    &ko_7_f7,
    &ko_8_f8,
    &ko_9_f9,
    &ko_0_f10,
    &ko_exlm_f11,
    &ko_at_f12,
    &ko_hash_f13,
    &ko_dlr_f14,
    &ko_perc_f15,
    &ko_circ_f16,
    &ko_ampr_f17,
    &ko_astr_f18,
    &ko_lprn_f19,
    &ko_rprn_f20,
    NULL  // Terminator
};

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