# =============================================================================
# Tutorial 10 — Viewport, Clipping & Culling
# =============================================================================
#
# What you'll learn:
#   - viewport(x, y, w, h) — renders into a sub-region of the screen
#   - clip_window(xmin, ymin, xmax, ymax) — clips to a rectangle
#   - The difference between viewport and clip
#   - cull_mode() — back-face culling based on winding order
#   - Winding order conventions (clockwise vs counter-clockwise)
#   - screen_size(), FB_W, FB_H constants
#   - Practical uses: split-screen, minimap, tiled rendering
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("10 — Viewport, Clipping & Culling")


def update(dt):
    state["t"] += dt


# =============================================================================
# HELPER: draw a test pattern to fill whatever viewport is current
# =============================================================================

def draw_test_pattern(color_offset=0.0):
    """Draw a grid of colored rectangles and triangles."""
    for row in range(4):
        for col in range(4):
            t = color_offset + row * 0.2 + col * 0.15
            r = 0.3 + 0.4 * math.sin(t)
            gr = 0.3 + 0.4 * math.sin(t + 2.1)
            b = 0.3 + 0.4 * math.sin(t + 4.2)
            x = col * (g.FB_W / 4.0) + 2.0
            y = row * (g.FB_H / 4.0) + 2.0
            w = g.FB_W / 4.0 - 4.0
            h = g.FB_H / 4.0 - 4.0
            g.rect(x, y, w, h, (r, gr, b))


# =============================================================================
# DRAW
# =============================================================================

def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))
    g.set_untextured()

    # =====================================================================
    # VIEWPORT vs CLIP WINDOW
    # =====================================================================
    # viewport(x, y, w, h) sets the rectangle where geometry is rasterized.
    # Coordinates are mapped so that vertex (0,0) maps to the viewport's
    # top-left and (FB_W, FB_H) maps to the viewport's bottom-right.
    #
    # clip_window(xmin, ymin, xmax, ymax) sets a scissor rectangle.
    # Pixels outside the clip rect are discarded.
    #
    # Key difference:
    #   viewport  — transforms coordinate space (geometry is stretched/shifted)
    #   clip      — just cuts off pixels outside the rectangle (no transform)
    #
    # begin_2d() resets both to full screen (0, 0, FB_W, FB_H).

    # =====================================================================
    # PART 1: SPLIT-SCREEN using viewport
    # =====================================================================
    # We'll divide the screen into left and right halves, each showing a
    # different test pattern.

    # Left half — viewport covers left 160 pixels
    g.viewport(0, 0, 160, 240)
    g.clip_window(0, 0, 160, 240)
    g.clear((0.02, 0.04, 0.08, 1.0))
    draw_test_pattern(t * 0.5)

    # Draw a label in the top-left of the left viewport
    g.rect(5.0, 5.0, 40.0, 6.0, (0.5, 0.7, 1.0))

    # Right half — viewport covers right 160 pixels
    g.viewport(160, 0, 160, 240)
    g.clip_window(160, 0, 320, 240)
    g.clear((0.08, 0.04, 0.02, 1.0))
    draw_test_pattern(t * 0.5 + 2.0)

    # Draw a label in the top-left of the right viewport
    g.rect(5.0, 5.0, 40.0, 6.0, (1.0, 0.7, 0.5))

    # Divider line — reset to full viewport to draw across both halves
    g.viewport(0, 0, g.FB_W, g.FB_H)
    g.clip_window(0, 0, g.FB_W, g.FB_H)
    g.rect(158.0, 0.0, 4.0, 240.0, (0.8, 0.8, 0.8))

    # =====================================================================
    # PART 2: CLIP WINDOW — drawing only within a sub-rectangle
    # =====================================================================
    # clip_window restricts where pixels can appear without changing
    # coordinate mapping. This is useful for UI panels, masking effects,
    # and rendering into a fixed region.

    # First draw a full-screen animated pattern
    angle = t * 0.8
    for i in range(8):
        a = angle + i * math.pi * 0.25
        x = 160.0 + math.cos(a) * 80.0
        y = 120.0 + math.sin(a) * 60.0
        r = 15.0 + math.sin(t + i) * 5.0
        # Draw only within a clipped region in the bottom-center
        g.clip_window(80, 150, 240, 235)
        g.rect(x - r, y - r, r * 2.0, r * 2.0,
               (0.5 + i * 0.05, 0.3, 1.0 - i * 0.05))

    # =====================================================================
    # PART 3: CULLING — back-face culling
    # =====================================================================
    # cull_mode() determines which triangles are discarded based on
    # their winding order (the direction the vertices go around).
    #
    #   CULL_DISABLE   — draw all triangles (default)
    #   CULL_NEGATIVE  — discard triangles with negative (CW) winding
    #   CULL_POSITIVE  — discard triangles with positive (CCW) winding
    #
    # Winding is determined by a 2D cross product:
    #   area = (v1.x-v0.x)*(v2.y-v0.y) - (v1.y-v0.y)*(v2.x-v0.x)
    #
    # Positive area = CCW, Negative area = CW
    #
    # This is useful for 3D rendering to avoid drawing back-facing polygons.

    # Reset viewport and clip for the culling demo
    g.viewport(0, 0, g.FB_W, g.FB_H)
    g.clip_window(0, 0, g.FB_W, g.FB_H)

    # Draw two triangles side by side — one CCW, one CW
    # First with culling DISABLED: both are visible
    g.cull_mode(g.CULL_DISABLE)
    g.rect(5.0, 150.0, 70.0, 12.0, (0.3, 0.3, 0.3))
    g.triangle(
        (25.0, 168.0, 0.4, 0.8, 0.4),   # CCW winding (positive area)
        (10.0, 198.0, 0.4, 0.8, 0.4),
        (55.0, 198.0, 0.4, 0.8, 0.4),
    )
    g.triangle(
        (70.0, 168.0, 0.8, 0.4, 0.4),   # CW winding (negative area)
        (35.0, 198.0, 0.8, 0.4, 0.4),   # (reversed vertex order)
        (50.0, 168.0, 0.8, 0.4, 0.4),
    )

    # Now with CULL_POSITIVE: only CW triangles visible
    # (green one disappears, red one stays)
    # We'll demonstrate this in a separate region below

    # =====================================================================
    # ANIMATED CULLING DEMO
    # =====================================================================
    # A rotating triangle that alternates between CW and CCW winding.
    # We'll show it with different cull modes.

    modes = [
        (g.CULL_DISABLE, "no cull"),
        (g.CULL_NEGATIVE, "cull CW"),
        (g.CULL_POSITIVE, "cull CCW"),
    ]

    for mi in range(len(modes)):
        mode = modes[mi][0]
        cx = 40.0 + mi * 120.0
        cy = 220.0
        g.cull_mode(mode)

        # Draw a rotating triangle
        for j in range(3):
            a0 = t * 1.5 + j * math.pi * 2.0 / 3.0
            a1 = t * 1.5 + (j + 1) * math.pi * 2.0 / 3.0
            g.triangle(
                (cx, cy, 1.0, 1.0, 1.0),
                (cx + math.cos(a0) * 12.0, cy + math.sin(a0) * 12.0,
                 0.4 + mi * 0.2, 0.6, 0.8 - mi * 0.2),
                (cx + math.cos(a1) * 12.0, cy + math.sin(a1) * 12.0,
                 0.4 + mi * 0.2, 0.6, 0.8 - mi * 0.2),
            )

    # Reset culling
    g.cull_mode(g.CULL_DISABLE)
