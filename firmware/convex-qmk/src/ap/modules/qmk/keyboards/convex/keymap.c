// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H




const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
      // 0        1        2        3        4        5        6        7        8        9        10       11       12       13       14       15       16       17       18       19       20       21
      {KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   _______, KC_F5,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  _______, _______, _______, _______, _______, _______, _______, _______},
      {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  _______, KC_BSPC, KC_INS,  KC_HOME, KC_PGUP, KC_NUM,  KC_PSLS, KC_PAST, KC_PMNS},
      {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    _______, KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_DEL,  KC_END,  KC_PGDN, KC_P7,   KC_P8,   KC_P9,   _______},
      {KC_CAPS, _______, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, _______, KC_ENT,  _______, _______, _______, KC_P4,   KC_P5,   KC_P6,   KC_PPLS},
      {KC_LSFT, _______, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, _______, KC_RSFT, _______, _______, KC_UP,   KC_P1,   KC_P2,   KC_P3,   _______},
      {KC_LCTL, _______, KC_LGUI, KC_LALT, KC_SPC,  _______, _______, KC_SPC,  _______, _______, KC_RALT, MO(1),   KC_RGUI, _______, KC_RCTL, KC_LEFT, KC_DOWN, KC_LEFT, _______, KC_P0,   KC_PDOT, KC_PENT}
    },        
};
