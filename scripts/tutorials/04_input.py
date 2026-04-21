# =============================================================================
# Tutorial 04 — Input & Interaction
# =============================================================================
#
# What you'll learn:
#   - Keyboard input: key_down() (held) vs key_pressed() (single frame)
#   - Mouse input: mouse_x(), mouse_y(), mouse_down()
#   - Callbacks: keydown(key), keyup(key)
#   - Quitting the app with quit()
#   - Building an interactive demo
#
# Glint provides two ways to read input:
#
#   Polling (check in update/draw):
#     g.key_down(key)      — is the key currently held down?
#     g.key_pressed(key)   — was the key just pressed this frame? (edge detect)
#     g.mouse_down(button) — is the mouse button held down?
#     g.mouse_x()          — mouse X in screen coordinates (0-319)
#     g.mouse_y()          — mouse Y in screen coordinates (0-239)
#
#   Callbacks (define these in your script):
#     keydown(key)         — called once when a key is pressed
#     keyup(key)           — called once when a key is released
#
# Key names: 'a'-'z', '0'-'9', 'space', 'enter', 'escape', 'tab',
#   'up', 'down', 'left', 'right', 'shift', 'ctrl', 'alt'
#
# Mouse buttons: 'left', 'right', 'middle' (or 0, 1, 2)
#
# =============================================================================

import math
import glide as g


state = {
    "px": 160.0,           # player x
    "py": 120.0,           # player y
    "speed": 100.0,        # pixels per second
    "trail": [],           # list of (x, y, age) trail particles
    "color_cycle": 0.0,    # animated color
    "click_rings": [],     # visual rings at click position
}


def load():
    g.set_title("04 — Input (arrows = move, click = ring, space = color, esc = quit)")


# -- CALLBACKS ----------------------------------------------------------------
# keydown(key) is called once when a key is first pressed (not repeated).
# keyup(key) is called once when a key is released.
# 'key' is a string like 'space', 'enter', 'a', 'up', etc.

def keydown(key):
    # Escape quits the application
    if key == "escape":
        g.quit()

    # Space cycles to a new random-ish color
    if key == "space":
        state["color_cycle"] += 1.5


# -- UPDATE -------------------------------------------------------------------

def update(dt):

    # =====================================================================
    # KEYBOARD — POLLED INPUT
    # =====================================================================
    # key_down(key) returns True while the key is held. Good for
    # continuous movement.

    dx = 0.0
    dy = 0.0
    if g.key_down("left"):
        dx -= 1.0
    if g.key_down("right"):
        dx += 1.0
    if g.key_down("up"):
        dy -= 1.0
    if g.key_down("down"):
        dy += 1.0

    # Normalize diagonal movement so it's not faster
    length = math.sqrt(dx * dx + dy * dy)
    if length > 0.0:
        dx /= length
        dy /= length

    state["px"] += dx * state["speed"] * dt
    state["py"] += dy * state["speed"] * dt

    # Clamp to screen bounds
    state["px"] = max(8.0, min(g.FB_W - 8.0, state["px"]))
    state["py"] = max(8.0, min(g.FB_H - 8.0, state["py"]))

    # =====================================================================
    # MOUSE
    # =====================================================================
    # mouse_x() and mouse_y() return screen-space pixel coordinates.
    # The fantasy screen is 320x240; the runtime scales mouse coords
    # from the window to the fantasy screen automatically.

    # Add trail particles while moving
    if length > 0.0:
        state["trail"].append((state["px"], state["py"], 0.0))

    # On mouse click, spawn an expanding ring at cursor position
    if g.mouse_down("left"):
        mx = float(g.mouse_x())
        my = float(g.mouse_y())
        state["click_rings"].append((mx, my, 0.0))

    # Update trail particles (age them and remove old ones)
    new_trail = []
    for x, y, age in state["trail"]:
        age += dt
        if age < 1.0:
            new_trail.append((x, y, age))
    state["trail"] = new_trail

    # Update click rings
    new_rings = []
    for x, y, age in state["click_rings"]:
        age += dt
        if age < 0.8:
            new_rings.append((x, y, age))
    state["click_rings"] = new_rings


# -- DRAW ---------------------------------------------------------------------

def draw():
    g.clear((0.03, 0.03, 0.08, 1.0))
    g.set_untextured()

    # =====================================================================
    # CLICK RINGS (expanding circles at mouse click location)
    # =====================================================================
    for x, y, age in state["click_rings"]:
        t = age / 0.8
        radius = 5.0 + t * 40.0
        alpha = 1.0 - t
        c = (0.3 + t * 0.7, 0.6, 1.0 - t * 0.5, alpha)

        # Build circle outline with line segments
        segments = 24
        for i in range(segments):
            a0 = (i / segments) * math.pi * 2.0
            a1 = ((i + 1) / segments) * math.pi * 2.0
            g.line(
                (x + math.cos(a0) * radius, y + math.sin(a0) * radius, c[0], c[1], c[2]),
                (x + math.cos(a1) * radius, y + math.sin(a1) * radius, c[0], c[1], c[2]),
            )

    # =====================================================================
    # TRAIL PARTICLES
    # =====================================================================
    for x, y, age in state["trail"]:
        alpha = 1.0 - age
        size = 3.0 * alpha
        c = (0.4, 0.7, 1.0, alpha)
        g.rect(x - size * 0.5, y - size * 0.5, size, size, c)

    # =====================================================================
    # PLAYER CURSOR — animated triangle that follows arrow keys
    # =====================================================================
    t = state["color_cycle"]
    cr = 0.5 + 0.5 * math.sin(t)
    cg = 0.5 + 0.5 * math.sin(t + 2.1)
    cb = 0.5 + 0.5 * math.sin(t + 4.2)
    color = (cr, cg, cb, 1.0)

    px = state["px"]
    py = state["py"]
    s = 8.0

    # Triangle pointing right
    g.triangle(
        (px + s, py,      color[0], color[1], color[2]),
        (px - s, py - s,  color[0], color[1], color[2]),
        (px - s, py + s,  color[0], color[1], color[2]),
    )

    # =====================================================================
    # MOUSE CURSOR — small crosshair at mouse position
    # =====================================================================
    mx = float(g.mouse_x())
    my = float(g.mouse_y())
    cs = 4.0
    g.line(
        (mx - cs, my, 0.8, 0.8, 0.8),
        (mx + cs, my, 0.8, 0.8, 0.8),
    )
    g.line(
        (mx, my - cs, 0.8, 0.8, 0.8),
        (mx, my + cs, 0.8, 0.8, 0.8),
    )

    # =====================================================================
    # HUD — status text using small rectangles as "pixel font"
    # =====================================================================
    # glint doesn't have a text renderer, so we'll just draw colored bars
    # as a simple status indicator.

    # Color indicator bar matching the player's color
    g.rect(4.0, 4.0, 40.0, 4.0, color)
    g.rect(4.0, 10.0, 40.0, 4.0, (0.3, 0.3, 0.3))
