# =============================================================================
# Tutorial 14 — 2D Camera & Parallax Scrolling
# =============================================================================
#
# What you'll learn:
#   - World-space vs screen-space coordinates
#   - A simple 2D camera (position + zoom)
#   - Transforming vertices through the camera before drawing
#   - Parallax scrolling with multiple background layers
#   - Tilemap rendering with frustum culling
#   - Drawing a world bigger than the screen
#
# Up until now, everything has been drawn in screen-space (0-319, 0-239).
# Real games have worlds much larger than the screen. A camera defines
# which part of the world is visible.
#
# The camera transform is simple:
#   screen_x = (world_x - cam_x) * zoom + screen_center_x
#   screen_y = (world_y - cam_y) * zoom + screen_center_y
#
# =============================================================================

import math
import glide as g


# =============================================================================
# WORLD DATA
# =============================================================================
# Our world is 800 pixels wide and 300 pixels tall.
# We'll scroll horizontally through it.

WORLD_W = 800.0
WORLD_H = 300.0
TILE_SIZE = 32.0

state = {
    "t": 0.0,
    "cam_x": 0.0,       # camera center in world space
    "cam_y": 150.0,     # camera center in world space
    "zoom": 1.0,
    "target_zoom": 1.0,
    "player_x": 200.0,
    "player_y": 180.0,
    "tex_tiles": -1,
    "tex_sky": -1,
    "tex_mountains": -1,
}


# =============================================================================
# PROCEDURAL TEXTURES
# =============================================================================

def make_tile_texture(size=32):
    """A dirt/grass tile with a grass top and dirt body."""
    pixels = []
    for y in range(size):
        for x in range(size):
            if y < 6:
                # Grass top
                noise = int(5 * math.sin(x * 1.5 + y * 0.5))
                pixels.extend([255, 40 + noise, 140 + noise, 40])
            else:
                # Dirt body
                noise = int(10 * math.sin(x * 0.8 + y * 0.3))
                pixels.extend([255, 120 + noise, 80 + noise, 40 + noise])
    return pixels


def make_sky_gradient(size=64):
    """A vertical gradient for the sky background."""
    pixels = []
    for y in range(size):
        for x in range(size):
            t = y / (size - 1)
            r = int(10 + t * 20)
            gr = int(15 + t * 30)
            b = int(40 + t * 60)
            pixels.extend([255, r, gr, b])
    return pixels


def make_mountain_texture(size=64):
    """A silhouette texture for distant mountains."""
    pixels = []
    for y in range(size):
        for x in range(size):
            # Mountain silhouette: higher in center, tapering at edges
            mx = abs(x - size // 2) / (size // 2)
            height = int((1.0 - mx * mx) * size * 0.7)
            if y > size - height:
                pixels.extend([255, 60, 50, 80])
            else:
                pixels.extend([0, 0, 0, 0])
    return pixels


# =============================================================================
# CAMERA TRANSFORM
# =============================================================================

def world_to_screen(wx, wy):
    """Convert world coordinates to screen coordinates."""
    sx = (wx - state["cam_x"]) * state["zoom"] + g.FB_W / 2.0
    sy = (wy - state["cam_y"]) * state["zoom"] + g.FB_H / 2.0
    return sx, sy


def screen_to_world(sx, sy):
    """Convert screen coordinates back to world coordinates."""
    wx = (sx - g.FB_W / 2.0) / state["zoom"] + state["cam_x"]
    wy = (sy - g.FB_H / 2.0) / state["zoom"] + state["cam_y"]
    return wx, wy


# =============================================================================
# TILEMAP
# =============================================================================
# A simple tilemap: 1 = ground tile, 0 = empty air.
# We build it procedurally so it's always valid.

def get_tile(tx, ty):
    """Return tile type at grid coordinates (tx, ty)."""
    if ty < 0 or ty >= 10:
        return 0
    # Ground level varies with a sine wave
    ground_h = 4 + int(2 * math.sin(tx * 0.3))
    if ty >= 10 - ground_h:
        return 1
    return 0


def draw_tilemap():
    """Draw all visible tiles, culling off-screen ones."""
    # Figure out which tiles are visible on screen
    tl_world = screen_to_world(0.0, 0.0)
    br_world = screen_to_world(float(g.FB_W), float(g.FB_H))

    min_tx = max(0, int(tl_world[0] // TILE_SIZE) - 1)
    max_tx = min(int(WORLD_W // TILE_SIZE), int(br_world[0] // TILE_SIZE) + 1)
    min_ty = max(0, int(tl_world[1] // TILE_SIZE) - 1)
    max_ty = min(int(WORLD_H // TILE_SIZE), int(br_world[1] // TILE_SIZE) + 1)

    g.set_mode("textured")
    g.tex_bind(state["tex_tiles"])

    for ty in range(min_ty, max_ty + 1):
        for tx in range(min_tx, max_tx + 1):
            if get_tile(tx, ty) == 0:
                continue
            wx = tx * TILE_SIZE
            wy = ty * TILE_SIZE
            sx, sy = world_to_screen(wx, wy)
            # Only draw if on screen (with a small margin)
            if sx + TILE_SIZE * state["zoom"] < -10 or sx > g.FB_W + 10:
                continue
            if sy + TILE_SIZE * state["zoom"] < -10 or sy > g.FB_H + 10:
                continue
            g.image(state["tex_tiles"], sx, sy,
                    TILE_SIZE * state["zoom"], TILE_SIZE * state["zoom"])


# =============================================================================
# PARALLAX LAYERS
# =============================================================================
# Parallax scrolling: distant layers move slower than the camera,
# creating an illusion of depth.
#
# Formula: layer_screen_x = (layer_world_x - cam_x * parallax_factor) * zoom

def draw_parallax_layer(tex, parallax, y_base, scale, color=(1.0, 1.0, 1.0, 1.0)):
    """Draw a repeating background layer with parallax scrolling."""
    # The layer's world position is offset by camera * parallax_factor
    # so it appears to move slower than foreground objects.
    layer_w = 64.0 * scale

    # Compute how many repetitions we need to fill the screen
    visible_left = state["cam_x"] * parallax - g.FB_W
    visible_right = state["cam_x"] * parallax + g.FB_W + layer_w

    start_idx = int(visible_left // layer_w) - 1
    end_idx = int(visible_right // layer_w) + 1

    g.set_mode("textured")
    g.set_blend_alpha()

    for i in range(start_idx, end_idx + 1):
        wx = i * layer_w
        # Parallax transform: the layer moves at parallax fraction of camera speed
        sx = (wx - state["cam_x"] * parallax) * state["zoom"] + g.FB_W / 2.0
        sy = y_base
        # Wrap horizontally by using fractional UVs
        u_offset = (i % 4) * 0.25
        g.image(tex, sx, sy, layer_w * state["zoom"], layer_w * state["zoom"] * 0.5,
                color=color, u0=u_offset, v0=0.0, u1=u_offset + 1.0, v1=1.0)


# =============================================================================
# LOAD
# =============================================================================

def load():
    g.set_title("14 — 2D Camera & Parallax (arrows = pan, z/x = zoom)")
    state["tex_tiles"] = g.upload_texture(32, 32, make_tile_texture(32))
    state["tex_sky"] = g.upload_texture(64, 64, make_sky_gradient(64))
    state["tex_mountains"] = g.upload_texture(64, 64, make_mountain_texture(64))


# =============================================================================
# UPDATE
# =============================================================================

def update(dt):
    t = state["t"] + dt
    state["t"] = t

    # Camera pan with arrow keys
    cam_speed = 120.0 * dt / state["zoom"]
    if g.key_down("left"):
        state["cam_x"] -= cam_speed
    if g.key_down("right"):
        state["cam_x"] += cam_speed
    if g.key_down("up"):
        state["cam_y"] -= cam_speed
    if g.key_down("down"):
        state["cam_y"] += cam_speed

    # Clamp camera to world bounds
    half_w = (g.FB_W / 2.0) / state["zoom"]
    half_h = (g.FB_H / 2.0) / state["zoom"]
    state["cam_x"] = max(half_w, min(WORLD_W - half_w, state["cam_x"]))
    state["cam_y"] = max(half_h, min(WORLD_H - half_h, state["cam_y"]))

    # Zoom with z (in) and x (out)
    if g.key_down("z"):
        state["target_zoom"] = min(3.0, state["target_zoom"] + dt * 2.0)
    if g.key_down("x"):
        state["target_zoom"] = max(0.5, state["target_zoom"] - dt * 2.0)

    # Smooth zoom
    state["zoom"] += (state["target_zoom"] - state["zoom"]) * min(1.0, dt * 10.0)

    # Animate player
    state["player_x"] = 200.0 + math.sin(t * 0.5) * 100.0
    state["player_y"] = 180.0 + math.sin(t * 1.2) * 10.0


# =============================================================================
# DRAW
# =============================================================================

def draw():
    g.clear((0.04, 0.06, 0.12, 1.0))

    # =====================================================================
    # PARALLAX BACKGROUNDS
    # =====================================================================
    # Draw from back to front: sky (slowest), mountains (medium), tiles (fast).

    # Sky layer — very slow parallax, full screen height
    draw_parallax_layer(state["tex_sky"], 0.1, 0.0, 4.0,
                        color=(1.0, 1.0, 1.0, 1.0))

    # Mountain layer — medium parallax, tinted slightly purple
    draw_parallax_layer(state["tex_mountains"], 0.3, 80.0, 3.0,
                        color=(0.8, 0.7, 1.0, 0.9))

    # =====================================================================
    # TILEMAP (foreground)
    # =====================================================================
    draw_tilemap()

    # =====================================================================
    # PLAYER
    # =====================================================================
    # Draw the player as a simple colored diamond in world space.

    g.set_untextured()
    g.set_blend_none()

    sx, sy = world_to_screen(state["player_x"], state["player_y"])
    size = 8.0 * state["zoom"]
    g.quad(
        g.vertex(sx, sy - size, color=(1.0, 0.4, 0.3, 1.0)),
        g.vertex(sx + size, sy, color=(1.0, 0.6, 0.4, 1.0)),
        g.vertex(sx, sy + size, color=(1.0, 0.4, 0.3, 1.0)),
        g.vertex(sx - size, sy, color=(1.0, 0.6, 0.4, 1.0)),
    )

    # Shadow beneath player
    shadow_sx, shadow_sy = world_to_screen(state["player_x"], state["player_y"] + 12.0)
    g.rect(shadow_sx - size, shadow_sy - 2.0, size * 2.0, 4.0,
           (0.0, 0.0, 0.0, 0.3))

    # =====================================================================
    # HUD
    # =====================================================================
    # Screen-space overlay that doesn't move with the camera.

    g.rect(4.0, 4.0, 80.0, 6.0, (0.15, 0.15, 0.2))
    g.rect(4.0, 4.0, 40.0 * state["zoom"], 6.0, (0.3, 0.7, 1.0))

    # Minimap: show the whole world as a tiny rectangle with camera box
    map_x, map_y = 260.0, 4.0
    map_w, map_h = 56.0, 20.0
    g.rect(map_x, map_y, map_w, map_h, (0.1, 0.1, 0.15))

    # Camera rect on minimap
    cam_rel_x = (state["cam_x"] - g.FB_W / 2.0 / state["zoom"]) / WORLD_W
    cam_rel_w = (g.FB_W / state["zoom"]) / WORLD_W
    g.rect(map_x + cam_rel_x * map_w, map_y + 2.0,
           cam_rel_w * map_w, map_h - 4.0, (0.5, 0.7, 1.0, 0.5))

    # Player dot on minimap
    player_mx = map_x + (state["player_x"] / WORLD_W) * map_w
    player_my = map_y + (state["player_y"] / WORLD_H) * map_h
    g.rect(player_mx - 1.0, player_my - 1.0, 2.0, 2.0, (1.0, 0.4, 0.3))
