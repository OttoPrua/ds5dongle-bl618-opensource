#include "../src/codex_keymap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    fails++;
}

static void pass(const char *msg)
{
    printf("ok  %s\n", msg);
}

static void zero_payload(uint8_t p[CK_PAYLOAD_LEN])
{
    memset(p, 0, CK_PAYLOAD_LEN);
    p[0] = p[1] = p[2] = p[3] = 128;
    p[7] = 0x08; /* hat idle */
    p[32] = 0x80; /* touch inactive */
    p[36] = 0x80;
}

static void set_face(uint8_t *p, int square, int cross, int circle, int tri)
{
    p[7] = (uint8_t)((p[7] & 0x0F)
        | (square ? 0x10 : 0) | (cross ? 0x20 : 0)
        | (circle ? 0x40 : 0) | (tri ? 0x80 : 0));
}

static void set_sh(uint8_t *p, int l1, int r1, int l2, int r2,
                   int create, int options, int l3, int r3)
{
    p[8] = (uint8_t)((l1 ? 1 : 0) | (r1 ? 2 : 0) | (l2 ? 4 : 0) | (r2 ? 8 : 0)
        | (create ? 0x10 : 0) | (options ? 0x20 : 0)
        | (l3 ? 0x40 : 0) | (r3 ? 0x80 : 0));
}

static int has_key(const ck_hid_t *h, uint8_t key)
{
    for (int i = 0; i < 6; i++)
        if (h->keys[i] == key) return 1;
    return 0;
}

static void expect_pulse(const char *name, uint8_t mods, uint8_t key)
{
    const ck_hid_t *h = ck_hid();
    if ((h->mods & mods) != mods || !has_key(h, key)) {
        fprintf(stderr, "FAIL %s: mods=0x%02x keys=%02x %02x (want mods 0x%02x key 0x%02x)\n",
                name, h->mods, h->keys[0], h->keys[1], mods, key);
        fails++;
    } else {
        pass(name);
    }
}

static void expect_none(const char *name)
{
    const ck_hid_t *h = ck_hid();
    if (h->mods || h->keys[0] || h->mouse_buttons || h->dx || h->dy || h->wheel || h->pan) {
        fprintf(stderr, "FAIL %s: mods=0x%02x key=0x%02x mb=0x%02x d=%d,%d w=%d p=%d\n",
                name, h->mods, h->keys[0], h->mouse_buttons, h->dx, h->dy, h->wheel, h->pan);
        fails++;
    } else {
        pass(name);
    }
}

int main(void)
{
    uint8_t p[CK_PAYLOAD_LEN];
    uint32_t t = 0;
    ck_init();

    /* Cross pulse Enter, hold does not keep Enter. */
    zero_payload(p);
    ck_tick(p, t += 8);
    set_face(p, 0, 1, 0, 0);
    ck_tick(p, t += 8);
    expect_pulse("cross enter", 0, CK_KEY_ENTER);
    ck_tick(p, t += 8); /* pulse up */
    ck_tick(p, t += 200);
    ck_tick(p, t += 200);
    if (has_key(ck_hid(), CK_KEY_ENTER))
        fail("cross hold still sending enter");
    else
        pass("cross hold does not repeat enter");
    zero_payload(p);
    ck_tick(p, t += 8);

    /* R1+square = copy, not backspace. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_face(p, 1, 0, 0, 0);
    ck_tick(p, t += 8);
    expect_pulse("R1+square copy", CK_MOD_LGUI, CK_KEY_C);
    if (has_key(ck_hid(), CK_KEY_BKSP))
        fail("R1+square also backspace");
    else
        pass("R1+square not backspace");
    zero_payload(p);
    ck_tick(p, t += 8);

    /* Square first, then R1: stays backspace, no copy on layer change. */
    ck_init(); t = 0;
    zero_payload(p);
    set_face(p, 1, 0, 0, 0);
    ck_tick(p, t += 8);
    if (!has_key(ck_hid(), CK_KEY_BKSP))
        fail("square should backspace");
    else
        pass("square backspace");
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    if (has_key(ck_hid(), CK_KEY_C))
        fail("latched backspace became copy");
    else
        pass("layer change does not retarget held square");
    zero_payload(p);
    ck_tick(p, t += 8);
    expect_none("release square+R1 no extra");

    /* R1+triangle paste, not undo. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_face(p, 0, 0, 0, 1);
    ck_tick(p, t += 8);
    expect_pulse("R1+triangle paste", CK_MOD_LGUI, CK_KEY_V);
    zero_payload(p); ck_tick(p, t += 8);

    /* R1+cross newline. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_face(p, 0, 1, 0, 0);
    ck_tick(p, t += 8);
    expect_pulse("R1+cross newline", CK_MOD_LSHIFT, CK_KEY_ENTER);
    zero_payload(p); ck_tick(p, t += 8);

    /* R1+R3 = Escape, not voice. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 1);
    ck_tick(p, t += 8);
    expect_pulse("R1+R3 escape", 0, CK_KEY_ESC);
    if (ck_hid()->voice_held)
        fail("R1+R3 also voice");
    else
        pass("R1+R3 not voice");
    zero_payload(p); ck_tick(p, t += 8);

    /* L1+R3 undefined: no voice, no esc. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 1, 0, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_sh(p, 1, 0, 0, 0, 0, 0, 0, 1);
    ck_tick(p, t += 8);
    if (ck_hid()->voice_held || has_key(ck_hid(), CK_KEY_ESC) || ck_hid()->keys[0])
        fail("L1+R3 should be idle");
    else
        pass("L1+R3 undefined");
    zero_payload(p); ck_tick(p, t += 8);

    /* Voice hold, then R1: stay voice, no escape. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 0, 0, 0, 0, 0, 0, 0, 1);
    ck_tick(p, t += 8);
    if (!ck_hid()->voice_held || !(ck_hid()->consumer & CK_CONSUMER_FN) || (ck_hid()->mods & CK_MOD_RCTRL))
        fail("R3 voice hold should be Fn, not Right Control");
    else
        pass("R3 voice hold Fn");
    set_sh(p, 0, 1, 0, 0, 0, 0, 0, 1);
    ck_tick(p, t += 8);
    if (has_key(ck_hid(), CK_KEY_ESC))
        fail("voice + R1 became escape");
    else
        pass("voice not retargeted by R1");
    if (!ck_hid()->voice_held)
        fail("voice dropped when R1 added");
    else
        pass("voice continues with R1");
    zero_payload(p); ck_tick(p, t += 8);

    /* R2 / L2 mouse with analog hysteresis. */
    ck_init(); t = 0;
    zero_payload(p);
    p[5] = 80;
    ck_tick(p, t += 8);
    if (!(ck_hid()->mouse_buttons & CK_MOUSE_LEFT))
        fail("R2 analog left button");
    else
        pass("R2 analog left button");
    p[5] = 30; /* between release 24 and press 40, still held */
    ck_tick(p, t += 8);
    if (!(ck_hid()->mouse_buttons & CK_MOUSE_LEFT))
        fail("R2 hysteresis hold");
    else
        pass("R2 hysteresis hold");
    p[5] = 10;
    ck_tick(p, t += 8);
    if (ck_hid()->mouse_buttons & CK_MOUSE_LEFT)
        fail("R2 release");
    else
        pass("R2 release");
    p[4] = 90;
    ck_tick(p, t += 8);
    if (!(ck_hid()->mouse_buttons & CK_MOUSE_RIGHT))
        fail("L2 right button");
    else
        pass("L2 right button");
    p[4] = 0;
    ck_tick(p, t += 8);

    /* Touchpad move, click bit ignored. */
    ck_init(); t = 0;
    zero_payload(p);
    p[32] = 0x01; /* active id 1 */
    p[33] = 100; p[34] = 0; p[35] = 0;
    ck_tick(p, t += 8);
    if (ck_hid()->dx || ck_hid()->dy)
        fail("first touch should origin-reset");
    else
        pass("touch origin reset");
    p[33] = 100 + 18;
    ck_tick(p, t += 8);
    if (ck_hid()->dx == 0)
        fail("touch should move");
    else
        pass("touch relative move");
    p[9] = 0x02; /* tp click */
    ck_tick(p, t += 8);
    if (ck_hid()->mouse_buttons)
        fail("touch click became mouse button");
    else
        pass("touch click ignored");
    zero_payload(p); ck_tick(p, t += 8);

    /* L1+R1 together: no random keys. */
    ck_init(); t = 0;
    zero_payload(p);
    set_sh(p, 1, 1, 0, 0, 0, 0, 0, 0);
    ck_tick(p, t += 8);
    set_face(p, 1, 1, 1, 1);
    ck_tick(p, t += 8);
    if (ck_hid()->keys[0] || ck_hid()->mods)
        fail("L1+R1 face buttons should be idle");
    else
        pass("L1+R1 no keyboard");
    p[5] = 90;
    ck_tick(p, t += 8);
    if (!(ck_hid()->mouse_buttons & CK_MOUSE_LEFT))
        fail("L1+R1 should keep mouse");
    else
        pass("L1+R1 mouse still works");

    if (fails) {
        fprintf(stderr, "\n%d failure(s)\n", fails);
        return 1;
    }
    printf("\nall tests passed\n");
    return 0;
}
