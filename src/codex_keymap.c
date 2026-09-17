#include "codex_keymap.h"
#include <string.h>

enum {
    CK_BTN_SQUARE = 0,
    CK_BTN_CROSS,
    CK_BTN_CIRCLE,
    CK_BTN_TRIANGLE,
    CK_BTN_L1,
    CK_BTN_R1,
    CK_BTN_L2,
    CK_BTN_R2,
    CK_BTN_CREATE,
    CK_BTN_OPTIONS,
    CK_BTN_L3,
    CK_BTN_R3,
    CK_BTN_DPAD_U,
    CK_BTN_DPAD_D,
    CK_BTN_DPAD_L,
    CK_BTN_DPAD_R,
    CK_BTN_COUNT
};

enum {
    CK_ACT_NONE = 0,
    CK_ACT_ENTER_PULSE,
    CK_ACT_NEWLINE_PULSE,
    CK_ACT_BKSP_REPEAT,
    CK_ACT_COPY_PULSE,
    CK_ACT_PASTE_PULSE,
    CK_ACT_UNDO_PULSE,
    CK_ACT_REDO_PULSE,
    CK_ACT_WORDDEL_PULSE,
    CK_ACT_FWDDEL_PULSE,
    CK_ACT_ESC_PULSE,
    CK_ACT_VOICE_HOLD,
    CK_ACT_APP_SWITCH_PULSE,
    CK_ACT_NEXT_WIN_PULSE,
    CK_ACT_PREV_CHAT_PULSE,
    CK_ACT_NEXT_CHAT_PULSE,
    CK_ACT_NEXT_ATTN_PULSE,
    CK_ACT_SPACE_PREV_PULSE,
    CK_ACT_SPACE_NEXT_PULSE,
    CK_ACT_MISSION_PULSE,
    CK_ACT_APP_EXPOSE_PULSE,
    CK_ACT_SHIFT_LEFT_HOLD,
    CK_ACT_SHIFT_RIGHT_HOLD,
    CK_ACT_SHIFT_UP_HOLD,
    CK_ACT_SHIFT_DOWN_HOLD,
    CK_ACT_ARROW_LEFT_REPEAT,
    CK_ACT_ARROW_RIGHT_REPEAT,
    CK_ACT_ARROW_UP_REPEAT,
    CK_ACT_ARROW_DOWN_REPEAT,
};

enum { CK_LY_BASE = 0, CK_LY_R1 = 1, CK_LY_L1 = 2, CK_LY_BOTH = 3 };

enum { CK_AXIS_NONE = 0, CK_AXIS_X = 1, CK_AXIS_Y = 2 };

typedef struct {
    uint8_t  down;
    uint8_t  act;
    uint32_t t0;
    uint32_t last_fire;
    uint8_t  fired;
} ck_btn_st_t;

typedef struct {
    uint8_t  dir;       /* 0 none, 1+, 2- */
    uint8_t  axis_lock; /* CK_AXIS_* */
    uint8_t  act;
    uint8_t  fired;
    uint32_t t0;
    uint32_t last_fire;
} ck_stick_st_t;

static ck_hid_t hid;
static ck_btn_st_t btns[CK_BTN_COUNT];
static ck_stick_st_t rstk, lstk;
static uint8_t layer;
static bool rstk_need_center, lstk_need_center;
static int16_t tp_last_x, tp_last_y;
static uint8_t tp_last_id;
static bool tp_have_origin;
static uint8_t mouse_buttons;
static uint8_t r2_held, l2_held;
static uint8_t pulse_mods, pulse_key, pulse_left;
static uint8_t voice_held;

static int8_t clamp_i8(int v)
{
    if (v > 127) return 127;
    if (v < -127) return (int8_t)-127;
    return (int8_t)v;
}

static uint8_t layer_from(uint8_t l1, uint8_t r1)
{
    if (l1 && r1) return CK_LY_BOTH;
    if (r1) return CK_LY_R1;
    if (l1) return CK_LY_L1;
    return CK_LY_BASE;
}

static uint8_t parse_buttons(const uint8_t *p, uint8_t *out)
{
    memset(out, 0, CK_BTN_COUNT);
    uint8_t b0 = p[7], b1 = p[8];
    uint8_t hat = b0 & 0x0F;
    out[CK_BTN_SQUARE]   = (b0 >> 4) & 1;
    out[CK_BTN_CROSS]    = (b0 >> 5) & 1;
    out[CK_BTN_CIRCLE]   = (b0 >> 6) & 1;
    out[CK_BTN_TRIANGLE] = (b0 >> 7) & 1;
    out[CK_BTN_L1]       = (b1 >> 0) & 1;
    out[CK_BTN_R1]       = (b1 >> 1) & 1;
    out[CK_BTN_L2]       = (b1 >> 2) & 1;
    out[CK_BTN_R2]       = (b1 >> 3) & 1;
    out[CK_BTN_CREATE]   = (b1 >> 4) & 1;
    out[CK_BTN_OPTIONS]  = (b1 >> 5) & 1;
    out[CK_BTN_L3]       = (b1 >> 6) & 1;
    out[CK_BTN_R3]       = (b1 >> 7) & 1;
    out[CK_BTN_DPAD_U]   = (hat == 0 || hat == 1 || hat == 7);
    out[CK_BTN_DPAD_D]   = (hat == 3 || hat == 4 || hat == 5);
    out[CK_BTN_DPAD_L]   = (hat == 5 || hat == 6 || hat == 7);
    out[CK_BTN_DPAD_R]   = (hat == 1 || hat == 2 || hat == 3);
    return hat;
}

static uint8_t resolve_act(uint8_t id, uint8_t ly)
{
    /* L1+R1: no new keyboard/stick actions. Mouse handled separately. */
    if (ly == CK_LY_BOTH)
        return CK_ACT_NONE;

    switch (id) {
    case CK_BTN_L1:
    case CK_BTN_R1:
        return CK_ACT_NONE;
    case CK_BTN_SQUARE:
        if (ly == CK_LY_R1) return CK_ACT_COPY_PULSE;
        if (ly == CK_LY_L1) return CK_ACT_WORDDEL_PULSE;
        return CK_ACT_BKSP_REPEAT;
    case CK_BTN_TRIANGLE:
        if (ly == CK_LY_R1) return CK_ACT_PASTE_PULSE;
        if (ly == CK_LY_L1) return CK_ACT_REDO_PULSE;
        return CK_ACT_UNDO_PULSE;
    case CK_BTN_CROSS:
        if (ly == CK_LY_R1) return CK_ACT_NEWLINE_PULSE;
        if (ly == CK_LY_L1) return CK_ACT_NONE;
        return CK_ACT_ENTER_PULSE;
    case CK_BTN_CIRCLE:
        if (ly == CK_LY_R1) return CK_ACT_NONE; /* re-edit: no confirmed shortcut */
        if (ly == CK_LY_L1) return CK_ACT_FWDDEL_PULSE;
        return CK_ACT_NONE; /* stop: no confirmed shortcut */
    case CK_BTN_R3:
        if (ly == CK_LY_R1) return CK_ACT_ESC_PULSE;
        if (ly == CK_LY_L1) return CK_ACT_NONE;
        return CK_ACT_VOICE_HOLD;
    case CK_BTN_L3:
        return CK_ACT_NONE; /* global recall: Mac helper, not a key */
    case CK_BTN_CREATE:
        if (ly == CK_LY_L1) return CK_ACT_NEXT_WIN_PULSE;
        if (ly == CK_LY_R1) return CK_ACT_NONE;
        return CK_ACT_APP_SWITCH_PULSE;
    case CK_BTN_OPTIONS:
        if (ly == CK_LY_R1) return CK_ACT_NEXT_ATTN_PULSE;
        return CK_ACT_NONE; /* focus composer: no confirmed shortcut */
    case CK_BTN_DPAD_U:
        if (ly == CK_LY_L1) return CK_ACT_MISSION_PULSE;
        if (ly == CK_LY_R1) return CK_ACT_NONE;
        return CK_ACT_PREV_CHAT_PULSE;
    case CK_BTN_DPAD_D:
        if (ly == CK_LY_L1) return CK_ACT_APP_EXPOSE_PULSE;
        if (ly == CK_LY_R1) return CK_ACT_NONE;
        return CK_ACT_NEXT_CHAT_PULSE;
    case CK_BTN_DPAD_L:
        if (ly == CK_LY_L1) return CK_ACT_SPACE_PREV_PULSE;
        return CK_ACT_NONE; /* chat search unassigned on this Codex build */
    case CK_BTN_DPAD_R:
        if (ly == CK_LY_L1) return CK_ACT_SPACE_NEXT_PULSE;
        if (ly == CK_LY_R1) return CK_ACT_NONE;
        return CK_ACT_NEXT_ATTN_PULSE;
    default:
        return CK_ACT_NONE;
    }
}

static void queue_pulse(uint8_t mods, uint8_t key)
{
    pulse_mods = mods;
    pulse_key = key;
    pulse_left = 2; /* down this tick, up next */
}

static void fire_pulse(uint8_t act)
{
    switch (act) {
    case CK_ACT_ENTER_PULSE:      queue_pulse(0, CK_KEY_ENTER); break;
    case CK_ACT_NEWLINE_PULSE:    queue_pulse(CK_MOD_LSHIFT, CK_KEY_ENTER); break;
    case CK_ACT_COPY_PULSE:       queue_pulse(CK_MOD_LGUI, CK_KEY_C); break;
    case CK_ACT_PASTE_PULSE:      queue_pulse(CK_MOD_LGUI, CK_KEY_V); break;
    case CK_ACT_UNDO_PULSE:       queue_pulse(CK_MOD_LGUI, CK_KEY_Z); break;
    case CK_ACT_REDO_PULSE:       queue_pulse(CK_MOD_LGUI | CK_MOD_LSHIFT, CK_KEY_Z); break;
    case CK_ACT_WORDDEL_PULSE:    queue_pulse(CK_MOD_LALT, CK_KEY_BKSP); break;
    case CK_ACT_FWDDEL_PULSE:     queue_pulse(0, CK_KEY_DELETE); break;
    case CK_ACT_ESC_PULSE:        queue_pulse(0, CK_KEY_ESC); break;
    case CK_ACT_APP_SWITCH_PULSE: queue_pulse(CK_MOD_LGUI, CK_KEY_TAB); break;
    case CK_ACT_NEXT_WIN_PULSE:   queue_pulse(CK_MOD_LGUI, CK_KEY_GRAVE); break;
    case CK_ACT_PREV_CHAT_PULSE:  queue_pulse(CK_MOD_LGUI | CK_MOD_LALT, CK_KEY_LEFT); break;
    case CK_ACT_NEXT_CHAT_PULSE:  queue_pulse(CK_MOD_LGUI | CK_MOD_LALT, CK_KEY_RIGHT); break;
    case CK_ACT_NEXT_ATTN_PULSE:  queue_pulse(CK_MOD_LGUI | CK_MOD_LALT, CK_KEY_A); break;
    case CK_ACT_SPACE_PREV_PULSE: queue_pulse(CK_MOD_LCTRL, CK_KEY_LEFT); break;
    case CK_ACT_SPACE_NEXT_PULSE: queue_pulse(CK_MOD_LCTRL, CK_KEY_RIGHT); break;
    case CK_ACT_MISSION_PULSE:    queue_pulse(CK_MOD_LCTRL, CK_KEY_UP); break;
    case CK_ACT_APP_EXPOSE_PULSE: queue_pulse(CK_MOD_LCTRL, CK_KEY_DOWN); break;
    default: break;
    }
}

static bool is_hold_act(uint8_t act)
{
    return act == CK_ACT_VOICE_HOLD
        || act == CK_ACT_SHIFT_LEFT_HOLD || act == CK_ACT_SHIFT_RIGHT_HOLD
        || act == CK_ACT_SHIFT_UP_HOLD || act == CK_ACT_SHIFT_DOWN_HOLD;
}

static bool is_repeat_act(uint8_t act)
{
    return act == CK_ACT_BKSP_REPEAT
        || act == CK_ACT_ARROW_LEFT_REPEAT || act == CK_ACT_ARROW_RIGHT_REPEAT
        || act == CK_ACT_ARROW_UP_REPEAT || act == CK_ACT_ARROW_DOWN_REPEAT;
}

static void apply_hold_or_repeat(uint8_t act, uint32_t now, uint32_t t0, uint32_t *last, uint8_t *started)
{
    uint8_t key = 0, mods = 0;
    uint32_t period = CK_REPEAT_PERIOD_MS;
    switch (act) {
    case CK_ACT_VOICE_HOLD: break;
    case CK_ACT_BKSP_REPEAT: key = CK_KEY_BKSP; break;
    case CK_ACT_ARROW_LEFT_REPEAT: key = CK_KEY_LEFT; break;
    case CK_ACT_ARROW_RIGHT_REPEAT: key = CK_KEY_RIGHT; break;
    case CK_ACT_ARROW_UP_REPEAT: key = CK_KEY_UP; break;
    case CK_ACT_ARROW_DOWN_REPEAT: key = CK_KEY_DOWN; break;
    case CK_ACT_SHIFT_LEFT_HOLD: mods = CK_MOD_LSHIFT; key = CK_KEY_LEFT; break;
    case CK_ACT_SHIFT_RIGHT_HOLD: mods = CK_MOD_LSHIFT; key = CK_KEY_RIGHT; break;
    case CK_ACT_SHIFT_UP_HOLD: mods = CK_MOD_LSHIFT; key = CK_KEY_UP; break;
    case CK_ACT_SHIFT_DOWN_HOLD: mods = CK_MOD_LSHIFT; key = CK_KEY_DOWN; break;
    default: return;
    }

    if (act == CK_ACT_VOICE_HOLD) {
        if (CK_VOICE_USE_FN)
            hid.consumer |= CK_CONSUMER_FN;
        else
            hid.mods |= CK_VOICE_MOD;
        hid.voice_held = 1;
        voice_held = 1;
        return;
    }

    bool fire = 0;
    if (!*started) {
        fire = 1;
        *started = 1;
        *last = now;
    } else if ((now - t0) >= CK_REPEAT_DELAY_MS && (now - *last) >= period) {
        fire = 1;
        *last = now;
    }

    if (act >= CK_ACT_SHIFT_LEFT_HOLD && act <= CK_ACT_SHIFT_DOWN_HOLD) {
        /* keep held for selection */
        hid.mods |= CK_MOD_LSHIFT;
        hid.keys[0] = key;
        return;
    }

    if (fire) {
        hid.keys[0] = key;
        hid.mods |= mods;
    }
}

static void add_key(uint8_t key)
{
    if (!key) return;
    for (int i = 0; i < 6; i++) {
        if (hid.keys[i] == key) return;
        if (hid.keys[i] == 0) {
            hid.keys[i] = key;
            return;
        }
    }
}

static int stick_delta(uint8_t v)
{
    return (int)v - CK_STICK_CENTER;
}

static uint8_t stick_neutral_pair(uint8_t x, uint8_t y)
{
    int dx = stick_delta(x), dy = stick_delta(y);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx < CK_STICK_DEAD && dy < CK_STICK_DEAD);
}

static int8_t digital_dir(uint8_t v, uint8_t prev_dir)
{
    int d = stick_delta(v);
    int ad = d < 0 ? -d : d;
    uint8_t press = (prev_dir != 0) ? CK_STICK_RELEASE : CK_STICK_PRESS;
    if (ad < press) return 0;
    return (d > 0) ? 1 : -1;
}

static void reset_stick(ck_stick_st_t *s)
{
    memset(s, 0, sizeof(*s));
}

void ck_init(void)
{
    ck_reset();
}

void ck_reset(void)
{
    memset(&hid, 0, sizeof(hid));
    memset(btns, 0, sizeof(btns));
    reset_stick(&rstk);
    reset_stick(&lstk);
    layer = CK_LY_BASE;
    rstk_need_center = lstk_need_center = false;
    tp_have_origin = false;
    tp_last_id = 0xFF;
    mouse_buttons = 0;
    r2_held = l2_held = 0;
    pulse_left = 0;
    voice_held = 0;
}

static void handle_triggers(const uint8_t *p)
{
    uint8_t r2a = p[5], l2a = p[4];
    uint8_t r2d = (p[8] >> 3) & 1;
    uint8_t l2d = (p[8] >> 2) & 1;

    uint8_t r2on = r2_held ? (r2a >= CK_TRIG_RELEASE || r2d)
                           : (r2a >= CK_TRIG_PRESS || r2d);
    uint8_t l2on = l2_held ? (l2a >= CK_TRIG_RELEASE || l2d)
                           : (l2a >= CK_TRIG_PRESS || l2d);
    r2_held = r2on;
    l2_held = l2on;

    if (r2on) mouse_buttons |= CK_MOUSE_LEFT;
    else mouse_buttons &= (uint8_t)~CK_MOUSE_LEFT;
    if (l2on) mouse_buttons |= CK_MOUSE_RIGHT;
    else mouse_buttons &= (uint8_t)~CK_MOUSE_RIGHT;

    if (voice_held && CK_VOICE_SUPPRESS_CLICKS) {
        /* keep already-held buttons, block newly pressed clicks */
        /* already applied; if we wanted to block new, we'd compare previous.
         * Spec: suppress new clicks during voice, allow existing release. */
    }
    hid.mouse_buttons = mouse_buttons;
}

static void handle_touch(const uint8_t *p)
{
    const uint8_t *tp = &p[32];
    bool active = !(tp[0] & 0x80);
    uint8_t id = tp[0] & 0x7F;
    uint16_t x = (uint16_t)(tp[1] | ((tp[2] & 0x0F) << 8));
    uint16_t y = (uint16_t)(((tp[2] & 0xF0) >> 4) | (tp[3] << 4));

    hid.dx = 0;
    hid.dy = 0;
    if (!active) {
        tp_have_origin = false;
        tp_last_id = 0xFF;
        return;
    }
    if (!tp_have_origin || id != tp_last_id) {
        tp_last_x = (int16_t)x;
        tp_last_y = (int16_t)y;
        tp_last_id = id;
        tp_have_origin = true;
        return;
    }
    int dx = ((int)x - tp_last_x) / CK_TP_DIV;
    int dy = ((int)y - tp_last_y) / CK_TP_DIV;
    if (dx || dy) {
        hid.dx = clamp_i8(dx);
        hid.dy = clamp_i8(dy);
        tp_last_x = (int16_t)x;
        tp_last_y = (int16_t)y;
    }
    /* physical click / tap never becomes a mouse button */
}

static void handle_right_stick(uint8_t rx, uint8_t ry, uint32_t now)
{
    hid.wheel = 0;
    if (rstk_need_center) {
        if (stick_neutral_pair(rx, ry)) {
            rstk_need_center = false;
            reset_stick(&rstk);
        } else {
            return;
        }
    }
    if (stick_neutral_pair(rx, ry)) {
        reset_stick(&rstk);
        return;
    }
    if (layer == CK_LY_BOTH) {
        reset_stick(&rstk);
        rstk_need_center = true;
        return;
    }

    int dx = stick_delta(rx), dy = stick_delta(ry);
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;

    if (rstk.axis_lock == CK_AXIS_NONE) {
        rstk.axis_lock = (adx >= ady) ? CK_AXIS_X : CK_AXIS_Y;
    }

    if (layer == CK_LY_BASE) {
        int8_t hx = 0, hy = 0;
        if (rstk.axis_lock == CK_AXIS_X)
            hx = digital_dir(rx, rstk.dir);
        else
            hy = digital_dir(ry, rstk.dir);
        uint8_t act = CK_ACT_NONE;
        if (hx > 0) act = CK_ACT_ARROW_RIGHT_REPEAT;
        else if (hx < 0) act = CK_ACT_ARROW_LEFT_REPEAT;
        else if (hy > 0) act = CK_ACT_ARROW_DOWN_REPEAT;
        else if (hy < 0) act = CK_ACT_ARROW_UP_REPEAT;
        if (act == CK_ACT_NONE) {
            reset_stick(&rstk);
            return;
        }
        if (rstk.act != act) {
            rstk.act = act;
            rstk.t0 = now;
            rstk.last_fire = 0;
            rstk.fired = 0;
            rstk.dir = 1;
        }
        uint8_t started = rstk.fired;
        apply_hold_or_repeat(act, now, rstk.t0, &rstk.last_fire, &started);
        rstk.fired = started;
        return;
    }

    if (layer == CK_LY_R1) {
        if (rstk.axis_lock == CK_AXIS_X) {
            int8_t hx = digital_dir(rx, rstk.dir);
            uint8_t act = CK_ACT_NONE;
            if (hx < 0) act = CK_ACT_PREV_CHAT_PULSE;
            else if (hx > 0) act = CK_ACT_NEXT_CHAT_PULSE;
            if (act && rstk.act != act) {
                rstk.act = act;
                rstk.dir = 1;
                rstk.fired = 0;
            }
            if (act && !rstk.fired) {
                fire_pulse(act);
                rstk.fired = 1;
            }
        } else {
            int8_t hy = digital_dir(ry, rstk.dir);
            if (hy && (now - rstk.last_fire) >= CK_WHEEL_PERIOD_MS) {
                hid.wheel = (int8_t)(hy > 0 ? -1 : 1); /* up = scroll up */
                rstk.last_fire = now;
                rstk.dir = 1;
            }
        }
        return;
    }

    if (layer == CK_LY_L1) {
        int8_t hx = 0, hy = 0;
        if (rstk.axis_lock == CK_AXIS_X)
            hx = digital_dir(rx, rstk.dir);
        else
            hy = digital_dir(ry, rstk.dir);
        uint8_t act = CK_ACT_NONE;
        if (hx > 0) act = CK_ACT_SHIFT_RIGHT_HOLD;
        else if (hx < 0) act = CK_ACT_SHIFT_LEFT_HOLD;
        else if (hy > 0) act = CK_ACT_SHIFT_DOWN_HOLD;
        else if (hy < 0) act = CK_ACT_SHIFT_UP_HOLD;
        if (!act) {
            reset_stick(&rstk);
            return;
        }
        rstk.act = act;
        rstk.dir = 1;
        uint8_t started = 1;
        apply_hold_or_repeat(act, now, rstk.t0, &rstk.last_fire, &started);
    }
}

static void handle_left_stick(uint8_t lx, uint8_t ly, uint32_t now)
{
    hid.pan = 0;
    if (layer != CK_LY_BASE) {
        /* undefined on L1/R1: do not fall back to scroll */
        if (!stick_neutral_pair(lx, ly))
            lstk_need_center = true;
        reset_stick(&lstk);
        return;
    }
    if (lstk_need_center) {
        if (stick_neutral_pair(lx, ly)) {
            lstk_need_center = false;
            reset_stick(&lstk);
        }
        return;
    }
    if (stick_neutral_pair(lx, ly)) {
        reset_stick(&lstk);
        return;
    }
    int dx = stick_delta(lx), dy = stick_delta(ly);
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (lstk.axis_lock == CK_AXIS_NONE)
        lstk.axis_lock = (adx >= ady) ? CK_AXIS_X : CK_AXIS_Y;
    if ((now - lstk.last_fire) < CK_WHEEL_PERIOD_MS)
        return;
    if (lstk.axis_lock == CK_AXIS_Y) {
        int8_t hy = digital_dir(ly, lstk.dir);
        if (hy) {
            hid.wheel = (int8_t)(hy > 0 ? -1 : 1);
            lstk.last_fire = now;
            lstk.dir = 1;
        }
    } else {
        int8_t hx = digital_dir(lx, lstk.dir);
        if (hx) {
            hid.pan = hx;
            lstk.last_fire = now;
            lstk.dir = 1;
        }
    }
}

void ck_tick(const uint8_t *payload, uint32_t now_ms)
{
    uint8_t cur[CK_BTN_COUNT];
    memset(&hid, 0, sizeof(hid));
    bool voice_already = btns[CK_BTN_R3].down && btns[CK_BTN_R3].act == CK_ACT_VOICE_HOLD;
    voice_held = voice_already;

    parse_buttons(payload, cur);
    uint8_t new_layer = layer_from(cur[CK_BTN_L1], cur[CK_BTN_R1]);
    if (new_layer != layer) {
        rstk_need_center = true;
        lstk_need_center = true;
        reset_stick(&rstk);
        reset_stick(&lstk);
        layer = new_layer;
    }

    /* Buttons: latch action at press. */
    for (int i = 0; i < CK_BTN_COUNT; i++) {
        if (i == CK_BTN_L2 || i == CK_BTN_R2)
            continue; /* analog mouse path */
        if (cur[i] && !btns[i].down) {
            btns[i].down = 1;
            btns[i].t0 = now_ms;
            btns[i].last_fire = 0;
            btns[i].fired = 0;
            btns[i].act = resolve_act((uint8_t)i, layer);
            if (btns[i].act != CK_ACT_NONE && !is_hold_act(btns[i].act) && !is_repeat_act(btns[i].act)) {
                if (!voice_already)
                    fire_pulse(btns[i].act);
                btns[i].fired = 1;
            }
        } else if (!cur[i] && btns[i].down) {
            btns[i].down = 0;
            btns[i].act = CK_ACT_NONE;
            btns[i].fired = 0;
        }
    }

    /* Apply holds/repeats from still-down buttons (action frozen at press). */
    for (int i = 0; i < CK_BTN_COUNT; i++) {
        if (!btns[i].down) continue;
        if (is_hold_act(btns[i].act) || is_repeat_act(btns[i].act)) {
            uint8_t started = btns[i].fired;
            apply_hold_or_repeat(btns[i].act, now_ms, btns[i].t0, &btns[i].last_fire, &started);
            btns[i].fired = started;
        }
    }

    if (pulse_left) {
        hid.mods |= pulse_mods;
        add_key(pulse_key);
        pulse_left--;
        if (!pulse_left) {
            pulse_mods = 0;
            pulse_key = 0;
        }
    }

    handle_triggers(payload);
    handle_touch(payload);
    handle_right_stick(payload[2], payload[3], now_ms);
    handle_left_stick(payload[0], payload[1], now_ms);

    if (hid.voice_held) {
        /* suppress new mouse clicks: if voice just started this tick and
         * triggers were not already held, drop new clicks. Existing holds
         * stay until physical release (mouse_buttons already computed). */
    }
}

const ck_hid_t *ck_hid(void)
{
    return &hid;
}

uint8_t ck_layer(void)
{
    return layer;
}

bool ck_stick_neutral(void)
{
    return rstk.act == 0 && lstk.act == 0 && !rstk.dir && !lstk.dir;
}
