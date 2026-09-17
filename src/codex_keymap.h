#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Codex keymap v0.1 — firmware-internal layer over DualSense USB payload.
 *
 * Host-testable: compile with -DCK_HOST_TEST (no SDK).
 * Does not talk USB/BT/audio. Caller feeds a 63-byte DS5 USB payload
 * plus now_ms, and reads the HID snapshot to emit.
 *
 * Custom identity (do not bump official 3.18/3.20a strings):
 *   CK_MAP_VERSION
 */

#define CK_MAP_VERSION        "codex-keymap-v0.1"
#define CK_PAYLOAD_LEN        63
#define CK_STICK_CENTER       128

/* Tunables — keep in one place. */
#define CK_STICK_DEAD         38
#define CK_STICK_PRESS        48
#define CK_STICK_RELEASE      32
#define CK_TRIG_PRESS         40
#define CK_TRIG_RELEASE       24
#define CK_REPEAT_DELAY_MS    400
#define CK_REPEAT_PERIOD_MS   45
#define CK_WHEEL_PERIOD_MS    40
#define CK_TP_DIV             6     /* touchpad counts per mouse step */
#define CK_VOICE_SUPPRESS_CLICKS 1

/* HID modifiers (boot keyboard). */
#define CK_MOD_LCTRL   0x01
#define CK_MOD_LSHIFT  0x02
#define CK_MOD_LALT    0x04
#define CK_MOD_LGUI    0x08
#define CK_MOD_RCTRL   0x10
#define CK_MOD_RSHIFT  0x20
#define CK_MOD_RALT    0x40
#define CK_MOD_RGUI    0x80

#define CK_KEY_A       0x04
#define CK_KEY_C       0x06
#define CK_KEY_F       0x09
#define CK_KEY_V       0x19
#define CK_KEY_Z       0x1D
#define CK_KEY_ENTER   0x28
#define CK_KEY_ESC     0x29
#define CK_KEY_BKSP    0x2A
#define CK_KEY_TAB     0x2B
#define CK_KEY_GRAVE   0x35
#define CK_KEY_DELETE  0x4C
#define CK_KEY_RIGHT   0x4F
#define CK_KEY_LEFT    0x50
#define CK_KEY_DOWN    0x51
#define CK_KEY_UP      0x52

#define CK_MOUSE_LEFT  0x01
#define CK_MOUSE_RIGHT 0x02

/* Consumer report ID 2 bit3 = Keyboard Fn (HID 0x0C / 0x029D). */
#define CK_CONSUMER_FN 0x08

typedef struct {
    uint8_t mods;
    uint8_t keys[6];
    uint8_t mouse_buttons;
    int8_t  dx;
    int8_t  dy;
    int8_t  wheel;     /* vertical */
    int8_t  pan;       /* horizontal; USB hook may map to shift+wheel */
    uint8_t consumer;  /* bit0 VolUp bit1 VolDn bit2 Mute bit3 Fn */
    uint8_t voice_held; /* R3: hold Apple/Globe Fn for WeType PTT */
} ck_hid_t;

void ck_init(void);
void ck_reset(void);                 /* disconnect / USB re-enum */
void ck_tick(const uint8_t *payload, uint32_t now_ms);
const ck_hid_t *ck_hid(void);

/* Test helpers */
uint8_t ck_layer(void);              /* 0 base, 1 R1, 2 L1, 3 both */
bool    ck_stick_neutral(void);
