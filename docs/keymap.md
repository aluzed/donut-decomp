# keymap.conf — control bindings

`keymap.conf` maps high-level **game actions** (walk forward, honk, pause…) to
physical inputs (keyboard keys, mouse buttons, gamepad buttons, gamepad axes).
Gameplay code only ever asks for actions, so anything in this file can be
rebound — including switching between keyboard and gamepad — without
recompiling.

You can rebind everything two ways:
- **In-game:** open the **Controls** menu, click a binding, then press the key,
  mouse button or gamepad input you want (Esc cancels). Changes are written to
  `keymap.conf` immediately, and there's a *Reset to defaults* button.
- **By hand:** edit `keymap.conf` directly using the syntax below.

## Location

`keymap.conf` is read from the working directory next to the other runtime
assets. If it is missing on startup the engine writes a default file (the table
below) so it can always be inspected and edited.

## File syntax

Plain UTF-8 text, one binding per line:

```
action = input [, input ...]
```

- Everything after `#` is a comment. Blank lines are ignored.
- The left-hand side is an **action id** (see the table). Unknown actions are
  skipped with a warning.
- The right-hand side is a comma-separated list of **input tokens**. Any of them
  triggers the action; for analog actions their magnitudes are combined.
- An action may appear on several lines; the inputs accumulate.
- Listing an action with an empty right-hand side leaves it unbound.

### Input tokens

**Keyboard / mouse** — the key name in lower case:

```
up down left right
a b c … z
0 1 2 … 9
space  enter  escape  tab  backspace
lshift rshift  lctrl rctrl  lalt ralt
mouse_left  mouse_right  mouse_middle
```

**Gamepad buttons** (SDL GameController, layout-independent):

```
pad_a pad_b pad_x pad_y
pad_lb pad_rb               # shoulder bumpers
pad_ls pad_rs               # stick clicks
pad_back pad_start pad_guide
pad_dpup pad_dpdown pad_dpleft pad_dpright
```

**Gamepad axes** — an axis token names one *half* of an analog axis, so a stick
drives two opposite actions. Sign follows raw SDL (`+` = right / down,
`-` = left / up). Triggers are positive-only.

```
pad_leftx+  pad_leftx-   pad_lefty+  pad_lefty-
pad_rightx+ pad_rightx-  pad_righty+ pad_righty-
pad_lt  pad_rt                          # left / right trigger
```

An optional per-binding dead zone is appended with `:`, e.g. `pad_leftx-:0.3`
(0..1, default `0.25`).

## Actions

| Action id | Meaning | Default keyboard | Default gamepad |
|---|---|---|---|
| `move_forward` | walk forward / throttle | `up` | `pad_lefty-`, `pad_rt` |
| `move_backward` | walk back / brake-reverse | `down` | `pad_lefty+`, `pad_lt` |
| `move_left` | strafe / steer left | `left` | `pad_leftx-` |
| `move_right` | strafe / steer right | `right` | `pad_leftx+` |
| `interact` | enter/exit vehicle, jump on foot | `e` | `pad_a` |
| `boost` | speed boost | `lshift` | `pad_rb` |
| `honk` | horn | `h` | `pad_x` |
| `vehicle_jump` | hop the vehicle | `space` | `pad_b` |
| `camera_look` | hold to capture mouse & look | `mouse_right` | `pad_rs` |
| `freecam_forward` | fly cam forward | `w` | — |
| `freecam_backward` | fly cam back | `s` | — |
| `freecam_left` | fly cam left | `a` | — |
| `freecam_right` | fly cam right | `d` | — |
| `freecam_up` | fly cam up | `space` | — |
| `freecam_down` | fly cam down | `lctrl` | — |
| `freecam_fast` | fly cam fast modifier | `lshift` | — |
| `ui_confirm` | advance splash / confirm | `space`, `enter` | `pad_a` |
| `ui_click` | menu primary click | `mouse_left` | — |
| `pause_toggle` | toggle pause | `escape` | `pad_start` |
| `quit_game` | quit from pause menu | `q` | `pad_back` |
| `reset_best_time` | reset best lap time | `r` | — |
| `restart_mission` | restart current mission | `m` | — |
| `toggle_debug_draw` | toggle debug overlay | `1` | — |
| `toggle_help` | toggle help overlay | `f` | — |
| `debug_teleport_to_vehicle` | warp to mission vehicle | `t` | — |

> Axis signs follow raw SDL (stick-up is `pad_lefty-`). Per-binding dead zones
> default to `0.25` and can be overridden with the `:value` suffix.

## Default keymap.conf

```ini
# donut control bindings — see docs/keymap.md
# action = input [, input ...]

# --- movement (on foot + driving) ---
move_forward  = up,    pad_lefty-, pad_rt
move_backward = down,  pad_lefty+, pad_lt
move_left     = left,  pad_leftx-
move_right    = right, pad_leftx+

# --- character / vehicle ---
interact      = e,      pad_a
boost         = lshift, pad_rb
honk          = h,      pad_x
vehicle_jump  = space,  pad_b

# --- free (debug) camera ---
camera_look     = mouse_right, pad_rs
freecam_forward = w
freecam_backward= s
freecam_left    = a
freecam_right   = d
freecam_up      = space
freecam_down    = lctrl
freecam_fast    = lshift

# --- ui / global ---
ui_confirm      = space, enter, pad_a
ui_click        = mouse_left
pause_toggle    = escape, pad_start
quit_game       = q, pad_back
reset_best_time = r
restart_mission = m
toggle_debug_draw = 1
toggle_help     = f

# --- debug ---
debug_teleport_to_vehicle = t
```
