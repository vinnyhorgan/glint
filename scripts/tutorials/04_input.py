# =============================================================================
# Tutorial 04 — Input & Interaction
# =============================================================================
#
# What you'll learn:
#   - Keyboard input: key_down() (held) vs key_pressed() (single frame)
#   - Mouse input: mouse_x(), mouse_y(), mouse_down(), mouse_pressed(),
#                  mouse_released()
#   - Callbacks: keydown(key), keyup(key)
#   - Quitting the app with quit()
#   - Building an interactive demo
#
# Glint provides two ways to read input:
#
#   Polling (check in update/draw):
#     g.key_down(key)       — is the key currently held down?
#     g.key_pressed(key)    — was the key just pressed this frame? (edge)
#     g.mouse_down(button)  — is the mouse button held down?
#     g.mouse_pressed(btn)  — was the mouse button just pressed this frame?
#     g.mouse_released(btn) — was the mouse button just released this frame?
#     g.mouse_x()           — mouse X in screen coordinates (0-319)
#     g.mouse_y()           — mouse Y in screen coordinates (0-239)
#
#   Callbacks (define these in your script):
#     keydown(key)          — called once when a key is pressed
#     keyup(key)            — called once when a key is released
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
    "burst_particles": [], # particles spawned on mouse press
    "held_color": (0.4, 0.7, 1.0),  # changes while mouse held
}


def load():
    g.set_title("04 — Input (arrows = move, click = burst, hold = rings, space = color, esc = quit)")


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
    #
    # For timing, you also have:
    #   g.time()  — returns the current time in seconds (since startup)
    #   g.dt()    — returns the delta time (same as the dt parameter above)

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
    #
    # mouse_down()     — True every frame while the button is held.
    # mouse_pressed()  — True for ONE frame when the button goes down.
    # mouse_released() — True for ONE frame when the button goes up.
    #
    # Use mouse_pressed() for single-shot actions (fire weapon, place block).
    # Use mouse_down() for continuous actions (spray paint, auto-fire).
    # Use mouse_released() for charge-up releases or drag-drop.

    mx = float(g.mouse_x())
    my = float(g.mouse_y())

    # -- Single-shot burst on press -----------------------------------------
    if g.mouse_pressed("left"):
        # Spawn a burst of 8 particles radiating outward
        for i in range(8):
            angle = (i / 8.0) * math.pi * 2.0
            spd = 60.0
            state["burst_particles"].append({
                "x": mx, "y": my,
                "vx": math.cos(angle) * spd,
                "vy": math.sin(angle) * spd,
                "age": 0.0,
                "color": (1.0, 0.8, 0.2),
            })

    # -- Continuous rings while held ----------------------------------------
    if g.mouse_down("left"):
        state["click_rings"].append((mx, my, 0.0))

    # -- Change cursor color while held -------------------------------------
    if g.mouse_down("left"):
        state["held_color"] = (1.0, 0.4, 0.3)
    elif g.mouse_released("left"):
        state["held_color"] = (0.4, 0.7, 1.0)

    # Add trail particles while moving
    if length > 0.0:
        state["trail"].append((state["px"], state["py"], 0.0))

    # Update trail particles in-place to avoid per-frame allocations
    # (good practice on a memory-constrained machine).
    for i in range(len(state["trail"]) - 1, -1, -1):
        x, y, age = state["trail"][i]
        age += dt
        if age >= 1.0:
            state["trail"].pop(i)
        else:
            state["trail"][i] = (x, y, age)

    # Update click rings in-place
    for i in range(len(state["click_rings"]) - 1, -1, -1):
        x, y, age = state["click_rings"][i]
        age += dt
        if age >= 0.8:
            state["click_rings"].pop(i)
        else:
            state["click_rings"][i] = (x, y, age)

    # Update burst particles in-place
    for i in range(len(state["burst_particles"]) - 1, -1, -1):
        p = state["burst_particles"][i]
        p["x"] += p["vx"] * dt
        p["y"] += p["vy"] * dt
        p["age"] += dt
        if p["age"] >= 0.5:
            state["burst_particles"].pop(i)


# -- DRAW ---------------------------------------------------------------------

def draw():
    g.clear((0.03, 0.03, 0.08, 1.0))
    g.set_mode("gouraud")

    # =====================================================================
    # TRANSPARENT EFFECTS (drawn first, behind the player)
    # =====================================================================
    g.set_blend_alpha()

    # -- Click rings (expanding circles at mouse click location) -------------
    # We use g.vertex() here because the 5-tuple format has no alpha
    # channel — only Vertex objects can carry per-vertex alpha.
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
                g.vertex(x + math.cos(a0) * radius, y + math.sin(a0) * radius, color=c),
                g.vertex(x + math.cos(a1) * radius, y + math.sin(a1) * radius, color=c),
            )

    # -- Trail particles -----------------------------------------------------
    for x, y, age in state["trail"]:
        alpha = 1.0 - age
        size = 3.0 * alpha
        c = (0.4, 0.7, 1.0, alpha)
        g.rect(x - size * 0.5, y - size * 0.5, size, size, c)

    # =====================================================================
    # OPAQUE PLAYER CURSOR
    # =====================================================================
    g.set_blend_none()

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
    # TRANSPARENT EFFECTS ON TOP OF PLAYER
    # =====================================================================
    g.set_blend_alpha()

    # -- Burst particles (spawned on mouse press) ----------------------------
    for p in state["burst_particles"]:
        t = p["age"] / 0.5
        alpha = 1.0 - t
        size = 3.0 * (1.0 - t)
        c = (p["color"][0], p["color"][1], p["color"][2], alpha)
        g.rect(p["x"] - size * 0.5, p["y"] - size * 0.5, size, size, c)

    # =====================================================================
    # OPAQUE UI OVERLAY
    # =====================================================================
    g.set_blend_none()

    # -- Mouse cursor — small crosshair at mouse position --------------------
    mx = float(g.mouse_x())
    my = float(g.mouse_y())
    cs = 4.0
    hc = state["held_color"]
    g.line(
        (mx - cs, my, hc[0], hc[1], hc[2]),
        (mx + cs, my, hc[0], hc[1], hc[2]),
    )
    g.line(
        (mx, my - cs, hc[0], hc[1], hc[2]),
        (mx, my + cs, hc[0], hc[1], hc[2]),
    )

    # =====================================================================
    # HUD — status text using small rectangles as "pixel font"
    # =====================================================================
    # glint doesn't have a text renderer, so we'll just draw colored bars
    # as a simple status indicator.

    # Color indicator bar matching the player's color
    g.rect(4.0, 4.0, 40.0, 4.0, color)
    g.rect(4.0, 10.0, 40.0, 4.0, (0.3, 0.3, 0.3))
