#include "codex_usb.h"
#include "codex_keymap.h"
#include "usb_gamepad.h"
#include <string.h>

static uint8_t prev_mods;
static uint8_t prev_keys[6];
static uint8_t prev_mb;
static uint8_t prev_cons;
static uint8_t have_prev;

void ck_usb_poll(void)
{
    const ck_hid_t *h = ck_hid();
    if (!usb_gamepad_kbd_ready())
        return;

    uint8_t kbd_ch = !have_prev || h->mods != prev_mods
        || memcmp(h->keys, prev_keys, 6) != 0;
    uint8_t mouse_ch = !have_prev || h->mouse_buttons != prev_mb
        || h->dx || h->dy || h->wheel || h->pan;
    uint8_t cons_ch = !have_prev || h->consumer != prev_cons;

    if (kbd_ch) {
        uint8_t r[8];
        memset(r, 0, sizeof(r));
        r[0] = h->mods;
        memcpy(r + 2, h->keys, 6);
        if (usb_gamepad_send_kbd_report(r, 8) == 0) {
            prev_mods = h->mods;
            memcpy(prev_keys, h->keys, 6);
            have_prev = 1;
        }
        return;
    }

    if (mouse_ch) {
        if (usb_gamepad_send_mouse_report(h->mouse_buttons, h->dx, h->dy,
                                          h->wheel, h->pan) == 0) {
            prev_mb = h->mouse_buttons;
            have_prev = 1;
        }
        return;
    }

    if (cons_ch) {
        if (usb_gamepad_send_consumer_report(h->consumer) == 0) {
            prev_cons = h->consumer;
            have_prev = 1;
        }
    }
}

void ck_usb_release_all(void)
{
    ck_reset();
    have_prev = 0;
    prev_mods = 0;
    memset(prev_keys, 0, sizeof(prev_keys));
    prev_mb = 0;
    prev_cons = 0;
    if (!usb_gamepad_kbd_ready())
        return;
    uint8_t z[8] = {0};
    usb_gamepad_send_kbd_report(z, 8);
}
