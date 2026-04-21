# =============================================================================
# Tutorial 12 — Bringing It All Together
# =============================================================================
#
# What you'll learn:
#   - How to combine every feature into a single cohesive scene
#   - Procedural texture generation for sprites
#   - Depth-sorted 3D rendering with fog
#   - Alpha blending for atmosphere
#   - Back-face culling for 3D geometry
#   - Viewport tricks for a HUD overlay
#   - Input for player movement
#   - A complete game-like render loop
#
# This tutorial ties together everything from tutorials 01-11:
#   01: Program structure, clear, triangle
#   02: Color, gouraud shading, rect
#   03: Shape builders (poly, circle)
#   04: Keyboard & mouse input
#   05: Alpha blending (transparent, additive)
#   06: Textures (upload, image, sprite sheet)
#   07: Depth buffer, pseudo-3D projection
#   08: Combine units (custom rendering modes)
#   09: Fog (atmospheric depth)
#   10: Viewport, clipping, culling
#   11: Alpha testing (cutouts)
#
# The result: a simple "fly through a canyon" scene with procedural
# textures, fog, a HUD, and player control.
#
# =============================================================================

import math
import glide as g


# =============================================================================
# STATE
# =============================================================================

state = {
    "t": 0.0,
    "px": 0.0,           # player X position (world space)
    "pz": 0.0,           # player Z position (world space, forward)
    "speed": 0.0,        # current forward speed
    "max_speed": 3.0,
    "tex_wall": -1,      # wall texture
    "tex_ground": -1,    # ground texture
}


# =============================================================================
# PROCEDURAL TEXTURES
# =============================================================================

def make_wall_texture(size=32):
    """Brick-like wall pattern."""
    pixels = []
    cell = 4
    for y in range(size):
        for x in range(size):
            row = y // cell
            col = x // cell
            offset = cell // 2 if row % 2 == 1 else 0
            is_mortar = (x + offset) % cell == 0 or y % cell == 0
            if is_mortar:
                pixels.extend([255, 40, 35, 35])
            else:
                base = 80 + ((row * 7 + col * 13) % 40)
                pixels.extend([255, base, base * 0.6, base * 0.4])
    return pixels


def make_ground_texture(size=32):
    """Grid pattern for ground."""
    pixels = []
    cell = 4
    for y in range(size):
        for x in range(size):
            is_line = x % cell == 0 or y % cell == 0
            if is_line:
                pixels.extend([255, 30, 50, 40])
            else:
                pixels.extend([255, 15, 25, 20])
    return pixels


# =============================================================================
# SHAPE HELPERS (from tutorial 03)
# =============================================================================

def poly(points, color):
    if len(points) < 3:
        return
    v0 = g.vertex(points[0][0], points[0][1], color=color)
    for i in range(1, len(points) - 1):
        v1 = g.vertex(points[i][0], points[i][1], color=color)
        v2 = g.vertex(points[i + 1][0], points[i + 1][1], color=color)
        g.triangle(v0, v1, v2)


def circle(cx, cy, radius, color, segments=16):
    points = []
    for i in range(segments):
        a = (i / segments) * math.pi * 2.0
        points.append((cx + math.cos(a) * radius, cy + math.sin(a) * radius))
    poly(points, color)


# =============================================================================
# SIMPLE 3D PROJECTION (from tutorial 07)
# =============================================================================

def project(wx, wy, wz, cam_x, cam_z):
    """Project world-space point to screen coordinates.

    World axes: X = right, Y = up, Z = forward (into screen).
    Camera is at (cam_x, 0, cam_z) looking along +Z.
    """
    rz = wz - cam_z
    rx = wx - cam_x
    ry = wy
    if rz < 0.2:
        return None
    focal = 160.0
    sx = 160.0 + rx * focal / rz
    sy = 120.0 - ry * focal / rz
    return (sx, sy, rz)


# =============================================================================
# LOAD
# =============================================================================

def load():
    g.set_title("12 — Canyon Flyer (arrows = move, space = quit)")

    # Upload textures (tutorial 06)
    state["tex_wall"] = g.upload_texture(
        32, 32, make_wall_texture(32),
        min_filter=g.TEXTUREFILTER_BILINEAR,
        s_clamp=g.TEXTURECLAMP_WRAP,
        t_clamp=g.TEXTURECLAMP_WRAP,
    )
    state["tex_ground"] = g.upload_texture(
        32, 32, make_ground_texture(32),
        min_filter=g.TEXTUREFILTER_POINT_SAMPLED,
    )


# =============================================================================
# KEY CALLBACKS (tutorial 04)
# =============================================================================

def keydown(key):
    if key == "space":
        g.quit()


# =============================================================================
# UPDATE (tutorial 04 — input handling)
# =============================================================================

def update(dt):
    t = state["t"]
    t += dt
    state["t"] = t

    # Movement
    if g.key_down("up"):
        state["speed"] = min(state["speed"] + dt * 4.0, state["max_speed"])
    elif g.key_down("down"):
        state["speed"] = max(state["speed"] - dt * 6.0, -0.5)
    else:
        state["speed"] *= 0.98

    if g.key_down("left"):
        state["px"] -= dt * 3.0
    if g.key_down("right"):
        state["px"] += dt * 3.0

    state["pz"] += state["speed"] * dt
    state["px"] = max(-6.0, min(6.0, state["px"]))


# =============================================================================
# DRAW
# =============================================================================

def draw():
    t = state["t"]
    px = state["px"]
    pz = state["pz"]
    speed = state["speed"]

    g.clear((0.02, 0.02, 0.06, 1.0))

    # =================================================================
    # 3D SCENE (tutorials 07, 09, 10)
    # =================================================================

    g.begin_3d()

    # -- Fog (tutorial 09) --
    g.fog_color_value(g.rgb(0.02, 0.02, 0.06))
    fog = g.make_fog_table(1.0, 12.0)
    g.fog_table(fog)
    g.fog_mode(g.FOG_WITH_TABLE)

    # -- Culling (tutorial 10) --
    g.cull_mode(g.CULL_NEGATIVE)

    # -- Ground plane (textured, tutorial 06) --
    g.set_mode("textured")
    g.tex_bind(state["tex_ground"])

    ground_start = int(pz) - 1
    for i in range(20):
        gz0 = ground_start + i
        gz1 = gz0 + 1
        p0 = project(-20.0, 0.0, gz0, px, pz)
        p1 = project(20.0, 0.0, gz0, px, pz)
        p2 = project(20.0, 0.0, gz1, px, pz)
        p3 = project(-20.0, 0.0, gz1, px, pz)
        if p0 and p1 and p2 and p3:
            v0 = (gz0 % 10) * 0.1
            v1_uv = ((gz0 + 1) % 10) * 0.1
            z_avg = (p0[2] + p1[2] + p2[2] + p3[2]) / 4.0
            z_norm = min(z_avg / 20.0, 1.0)
            g.quad(
                g.vertex(p0[0], p0[1], z=z_norm, u=0.0, v=v0),
                g.vertex(p1[0], p1[1], z=z_norm, u=1.0, v=v0),
                g.vertex(p2[0], p2[1], z=z_norm, u=1.0, v=v1_uv),
                g.vertex(p3[0], p3[1], z=z_norm, u=0.0, v=v1_uv),
            )

    # -- Canyon walls (textured, tutorials 06, 07, 10) --
    g.set_mode("textured_gouraud")
    g.tex_bind(state["tex_wall"])

    wall_x_left = -8.0
    wall_x_right = 8.0
    wall_height = 5.0

    for i in range(20):
        gz0 = ground_start + i
        gz1 = gz0 + 1

        # Left wall
        p0 = project(wall_x_left, 0.0, gz0, px, pz)
        p1 = project(wall_x_left, 0.0, gz1, px, pz)
        p2 = project(wall_x_left, wall_height, gz1, px, pz)
        p3 = project(wall_x_left, wall_height, gz0, px, pz)
        if p0 and p1 and p2 and p3:
            z_norm = min((p0[2] + p2[2]) / 40.0, 1.0)
            brightness = 0.5 + 0.3 * math.sin(gz0 * 0.5 + t)
            c = (brightness, brightness * 0.8, brightness * 0.6)
            g.quad(
                g.vertex(p0[0], p0[1], z=z_norm, color=c, u=0.0, v=0.0),
                g.vertex(p1[0], p1[1], z=z_norm, color=c, u=0.0, v=1.0),
                g.vertex(p2[0], p2[1], z=z_norm, color=c, u=1.0, v=1.0),
                g.vertex(p3[0], p3[1], z=z_norm, color=c, u=1.0, v=0.0),
            )

        # Right wall
        p0 = project(wall_x_right, 0.0, gz0, px, pz)
        p1 = project(wall_x_right, 0.0, gz1, px, pz)
        p2 = project(wall_x_right, wall_height, gz1, px, pz)
        p3 = project(wall_x_right, wall_height, gz0, px, pz)
        if p0 and p1 and p2 and p3:
            z_norm = min((p0[2] + p2[2]) / 40.0, 1.0)
            brightness = 0.5 + 0.3 * math.sin(gz0 * 0.5 + t + 1.0)
            c = (brightness * 0.6, brightness * 0.7, brightness)
            g.quad(
                g.vertex(p0[0], p0[1], z=z_norm, color=c, u=0.0, v=0.0),
                g.vertex(p1[0], p1[1], z=z_norm, color=c, u=0.0, v=1.0),
                g.vertex(p2[0], p2[1], z=z_norm, color=c, u=1.0, v=1.0),
                g.vertex(p3[0], p3[1], z=z_norm, color=c, u=1.0, v=0.0),
            )

    # -- Floating crystals (tutorials 02, 05, 07) --
    g.cull_mode(g.CULL_DISABLE)
    g.fog_mode(g.FOG_DISABLE)
    g.set_untextured()
    g.set_blend_add()

    for i in range(6):
        crystal_z = pz + 3.0 + i * 3.0
        crystal_x = math.sin(crystal_z * 0.4 + i) * 4.0
        crystal_y = 1.5 + math.sin(t * 1.5 + i * 2.0) * 0.5

        p = project(crystal_x, crystal_y, crystal_z, px, pz)
        if p is None:
            continue

        sx, sy, sz = p
        # Scale based on distance
        scale = 160.0 / sz
        cr = 3.0 * scale

        if cr < 1.0:
            continue

        # Glow (tutorial 05 — additive blending)
        phase = t * 2.0 + i * 1.5
        r = 0.5 + 0.5 * math.sin(phase)
        gr = 0.3 + 0.3 * math.sin(phase + 2.0)
        b = 0.8 + 0.2 * math.sin(phase + 4.0)

        # Diamond shape (tutorial 03)
        poly([
            (sx, sy - cr),
            (sx + cr * 0.7, sy),
            (sx, sy + cr),
            (sx - cr * 0.7, sy),
        ], (r, gr, b, 0.4))

        # Core
        circle(sx, sy, cr * 0.3, (r + 0.3, gr + 0.3, b + 0.2, 0.6))

    # =================================================================
    # 2D HUD OVERLAY (tutorials 02, 10)
    # =================================================================

    g.begin_2d()
    g.set_blend_none()
    g.set_untextured()
    g.fog_mode(g.FOG_DISABLE)
    g.cull_mode(g.CULL_DISABLE)

    # Speed bar (tutorial 02 — flat color rects)
    g.rect(10.0, 226.0, 100.0, 8.0, (0.15, 0.15, 0.2))
    speed_frac = abs(speed) / state["max_speed"]
    bar_color = (0.2 + speed_frac * 0.8, 0.8 - speed_frac * 0.5, 0.3)
    g.rect(10.0, 226.0, 100.0 * speed_frac, 8.0, bar_color)

    # Position indicator
    g.rect(130.0, 226.0, 60.0, 8.0, (0.15, 0.15, 0.2))
    pos_frac = (state["px"] + 6.0) / 12.0
    g.rect(130.0 + pos_frac * 52.0, 226.0, 8.0, 8.0, (0.5, 0.7, 1.0))

    # Altitude scan lines (tutorial 05 — additive blending)
    g.set_blend_add()
    for i in range(3):
        y = 10.0 + i * 6.0 + math.sin(t * 3.0 + i) * 2.0
        alpha = 0.1 + 0.05 * math.sin(t * 2.0 + i)
        g.rect(0.0, y, 320.0, 1.0, (0.2, 0.4, 0.8, alpha))
    g.set_blend_none()

    # Crosshair (tutorial 04 — lines)
    cx, cy = 160.0, 120.0
    g.line((cx - 6, cy, 0.4, 0.6, 0.8), (cx - 2, cy, 0.4, 0.6, 0.8))
    g.line((cx + 2, cy, 0.4, 0.6, 0.8), (cx + 6, cy, 0.4, 0.6, 0.8))
    g.line((cx, cy - 6, 0.4, 0.6, 0.8), (cx, cy - 2, 0.4, 0.6, 0.8))
    g.line((cx, cy + 2, 0.4, 0.6, 0.8), (cx, cy + 6, 0.4, 0.6, 0.8))
