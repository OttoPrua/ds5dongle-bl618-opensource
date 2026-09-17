# Codex keymap v0.1

Firmware-internal DualSense layer for Mac / Codex. Does not replace the BT/USB/audio stacks.

Identity string: `codex-keymap-v0.1` (`CK_MAP_VERSION`). Do **not** report this as upstream 3.19/3.20/3.21.

## Host tests (no board, no SDK)

```bash
cd host_test && make test
```

Input is a DualSense USB 63-byte payload sequence. Output is the expected HID keyboard/mouse snapshot.

## Layer rules

- L1 / R1 are firmware layers, not Mac modifiers.
- Action is latched at press and kept until that button releases.
- Undefined combos (including L1+R3) emit nothing. They do not fall back to the base layer.
- L1+R1 together: no new keyboard/stick actions; mouse buttons stay as R2/L2.
- Touchpad: relative move only. Light tap, double tap, and physical click never become mouse buttons.
- R2 / L2: left / right mouse with analog hysteresis.
- ✕ Enter is a pulse; holding ✕ does not keep Enter down.

## Mac shortcuts used (document vs live)

| Intent | Firmware HID | Status |
| --- | --- | --- |
| Copy / paste / undo / redo | ⌘C / ⌘V / ⌘Z / ⌘⇧Z | standard Mac |
| Word delete / forward delete | ⌥⌫ / Delete | standard Mac |
| Prev / next chat | ⌘⌥← / ⌘⌥→ | documented, not live-tested |
| Next needing attention | ⌘⌥A | documented, not live-tested |
| App switch / next window | ⌘Tab / ⌘` | standard Mac |
| Spaces / Mission Control | ⌃← ⌃→ / ⌃↑ / ⌃↓ | standard Mac |
| WeType voice | Hold **Right Control** on R3 | Default. Change `CK_VOICE_USE_FN` later for Consumer Fn. |
| Stop run / re-edit / focus composer / L3 recall | none | no confirmed native shortcut yet |

Set WeType 「按住说话」to **右 Control**, matching R3. Your keyboard has no Fn; the firmware default is a real HID modifier you can bind in WeType. Swap later with `CK_VOICE_USE_FN` in `codex_keymap.h` (Consumer `0x029D`). Do not spoof Apple VID.

## Do not flash this tree over 3.20aH

Board reports `LCT616-DS5 3.20aH`. Public git is still `LCT616-DS5 3.18`. Config report version on device is 3; public `CONFIG_VERSION` is 2.
