# =============================================================================
# Tutorial 13 — Sprite Animation
# =============================================================================
#
# What you'll learn:
#   - Frame-based sprite animation from a sprite sheet
#   - Animation timing: fps, frame duration, looping
#   - Animation state machine (idle / walk / jump)
#   - Sprite flipping (facing left vs right) with UV mirroring
#   - Drawing sprites with image() and custom UV sub-regions
#   - Organizing sprite data cleanly
#
# Tutorial 06 showed how to select a single sub-region from a texture.
# This tutorial goes further: cycling through many sub-regions over time
# to create the illusion of motion.
#
# =============================================================================

import math
import glide as g


# =============================================================================
# SPRITE SHEET DATA
# =============================================================================
# We'll build a 4x4 sprite sheet with 16 frames:
#   Row 0: idle frames (4 frames)
#   Row 1: walk frames (4 frames)
#   Row 2: jump frames (2 frames)
#   Row 3: unused / extra frames
#
# Each cell is 16x16 pixels, so the sheet is 64x64.

state = {
    "t": 0.0,
    "sheet": -1,
    "player_x": 160.0,
    "player_y": 160.0,
    "facing_right": True,
    "anim_state": "idle",   # "idle", "walk", "jump"
    "anim_timer": 0.0,
    "ground_y": 160.0,
    "vy": 0.0,
    "on_ground": True,
}


# Animation configuration: which row, how many frames, fps
ANIM_CONFIG = {
    "idle": {"row": 0, "frames": 4, "fps": 4.0},
    "walk": {"row": 1, "frames": 4, "fps": 8.0},
    "jump": {"row": 2, "frames": 2, "fps": 4.0},
}


def make_sprite_sheet(size=64):
    """Generate a 4x4 sprite sheet with simple character frames."""
    cell = size // 4
    pixels = []
    for y in range(size):
        for x in range(size):
            cx = x // cell
            cy = y // cell
            # Frame-local coordinates
            lx = x % cell
            ly = y % cell
            # Simple shapes per frame
            if cy == 0:
                # Idle: pulsing circle (body) + small head
                dx = lx - cell // 2
                dy = ly - cell // 2
                d = math.sqrt(dx * dx + dy * dy)
                pulse = 0.8 + 0.2 * math.sin(cx * 1.5)
                if d < (cell // 3) * pulse:
                    r = int(200 + 55 * math.sin(cx))
                    gr = int(180 + 40 * math.cos(cx))
                    b = int(100 + 30 * math.sin(cx + 1))
                    pixels.extend([255, r, gr, b])
                else:
                    pixels.extend([0, 0, 0, 0])
            elif cy == 1:
                # Walk: offset circle showing stride
                offset = (cx - 1.5) * 2.0
                dx = lx - cell // 2 + offset
                dy = ly - cell // 2
                d = math.sqrt(dx * dx + dy * dy)
                if d < cell // 3:
                    r = int(200 + 30 * cx)
                    gr = int(160 + 20 * cx)
                    b = int(80 + 15 * cx)
                    pixels.extend([255, r, gr, b])
                else:
                    pixels.extend([0, 0, 0, 0])
            elif cy == 2:
                # Jump: stretched shape
                dx = (lx - cell // 2) * 0.7
                dy = (ly - cell // 2) * 1.3
                d = math.sqrt(dx * dx + dy * dy)
                if d < cell // 3:
                    r = int(220 - cx * 20)
                    gr = int(200 - cx * 15)
                    b = int(120)
                    pixels.extend([255, r, gr, b])
                else:
                    pixels.extend([0, 0, 0, 0])
            else:
                # Extra: simple star
                dx = lx - cell // 2
                dy = ly - cell // 2
                d = abs(dx) + abs(dy)
                if d < cell // 2 + cx:
                    pixels.extend([255, 255, 220, 80])
                else:
                    pixels.extend([0, 0, 0, 0])
    return pixels


def current_frame_uvs():
    """Return (u0, v0, u1, v1) for the current animation frame."""
    cfg = ANIM_CONFIG[state["anim_state"]]
    row = cfg["row"]
    num_frames = cfg["frames"]
    fps = cfg["fps"]

    # Which frame in the animation?
    frame = int(state["anim_timer"] * fps) % num_frames

    # Convert to UVs (sheet is 4x4)
    cell_uv = 1.0 / 4.0
    u0 = frame * cell_uv
    u1 = u0 + cell_uv
    v0 = row * cell_uv
    v1 = v0 + cell_uv

    # Flip horizontally if facing left
    if not state["facing_right"]:
        u0, u1 = u1, u0

    return u0, v0, u1, v1


def load():
    g.set_title("13 — Sprite Animation (arrows = move, space = jump)")
    state["sheet"] = g.upload_texture(64, 64, make_sprite_sheet(64))


def update(dt):
    t = state["t"] + dt
    state["t"] = t

    # =====================================================================
    # INPUT & PHYSICS
    # =====================================================================
    # Arrow keys move the player. Space jumps.
    # The animation state changes based on what the player is doing.

    dx = 0.0
    if g.key_down("left"):
        dx -= 1.0
        state["facing_right"] = False
    if g.key_down("right"):
        dx += 1.0
        state["facing_right"] = True

    # Jump
    if g.key_pressed("space") and state["on_ground"]:
        state["vy"] = -180.0
        state["on_ground"] = False

    # Gravity
    state["vy"] += 400.0 * dt
    state["player_y"] += state["vy"] * dt

    # Ground collision
    if state["player_y"] >= state["ground_y"]:
        state["player_y"] = state["ground_y"]
        state["vy"] = 0.0
        state["on_ground"] = True

    # Horizontal movement
    state["player_x"] += dx * 80.0 * dt
    state["player_x"] = max(16.0, min(g.FB_W - 16.0, state["player_x"]))

    # =====================================================================
    # ANIMATION STATE MACHINE
    # =====================================================================
    # Determine which animation to play based on player state:
    #   jumping  -> "jump" (highest priority)
    #   moving   -> "walk"
    #   still    -> "idle"

    new_state = state["anim_state"]
    if not state["on_ground"]:
        new_state = "jump"
    elif dx != 0.0:
        new_state = "walk"
    else:
        new_state = "idle"

    # If the state changed, reset the timer so the animation starts fresh
    if new_state != state["anim_state"]:
        state["anim_state"] = new_state
        state["anim_timer"] = 0.0
    else:
        state["anim_timer"] += dt


def draw():
    g.clear((0.05, 0.05, 0.12, 1.0))

    # =====================================================================
    # BACKGROUND
    # =====================================================================
    # A simple ground line and sky gradient for context.

    g.set_untextured()
    g.set_blend_none()

    # Sky
    for i in range(12):
        y = i * 12.0
        brightness = 0.04 + i * 0.008
        g.rect(0.0, y, float(g.FB_W), 12.0, (brightness, brightness, brightness * 1.5))

    # Ground
    g.rect(0.0, state["ground_y"] + 8.0, float(g.FB_W),
           float(g.FB_H) - state["ground_y"] - 8.0,
           (0.12, 0.10, 0.08))

    # =====================================================================
    # DRAW THE SPRITE
    # =====================================================================
    # image() with custom UVs draws one frame from the sprite sheet.
    # We scale the 16x16 pixel frame up to 32x32 screen pixels for
    # visibility on the 320x240 screen.

    g.set_mode("transparent")
    g.set_blend_alpha()

    u0, v0, u1, v1 = current_frame_uvs()
    g.image(state["sheet"],
            state["player_x"] - 16.0, state["player_y"] - 16.0,
            32.0, 32.0,
            u0=u0, v0=v0, u1=u1, v1=v1)

    # =====================================================================
    # STATE INDICATOR
    # =====================================================================
    # A small colored bar at the top-left shows which animation is playing.

    g.set_untextured()
    g.set_blend_none()
    colors = {"idle": (0.3, 0.8, 0.4), "walk": (0.8, 0.6, 0.2), "jump": (0.4, 0.5, 0.9)}
    c = colors[state["anim_state"]]
    g.rect(4.0, 4.0, 40.0, 6.0, c)

    # =====================================================================
    # MULTI-SPRITE DEMO
    # =====================================================================
    # Show all animation frames side-by-side as a "film strip" reference.

    g.set_mode("transparent")
    g.set_blend_alpha()

    strip_y = 210.0
    for row, (name, cfg) in enumerate(ANIM_CONFIG.items()):
        for frame in range(cfg["frames"]):
            cell_uv = 1.0 / 4.0
            fu0 = frame * cell_uv
            fu1 = fu0 + cell_uv
            fv0 = cfg["row"] * cell_uv
            fv1 = fv0 + cell_uv
            x = 5.0 + frame * 18.0
            y = strip_y + row * 18.0
            g.image(state["sheet"], x, y, 16.0, 16.0,
                    u0=fu0, v0=fv0, u1=fu1, v1=fv1)
