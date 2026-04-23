# =============================================================================
# Tutorial 02 — Color & Gouraud Shading
# =============================================================================
#
# What you'll learn:
#   - Per-vertex color and gouraud shading (smooth color interpolation)
#   - Color helper functions: rgba(), rgb(), color_pack(), color_unpack()
#   - The rect() helper for filled rectangles
#   - set_mode('gouraud') vs set_mode('flat')
#   - constant_color_value() for flat-colored geometry
#   - Using a 0-255 color range for convenience
#
# In tutorial 01 we drew a white triangle with identical colors on each
# vertex. When the three vertices have *different* colors, the GPU
# interpolates (blends) them across the surface — this is called
# "Gouraud shading" and it's the default mode.
#
# =============================================================================

import math
import glide as g


# -- Color helpers ------------------------------------------------------------
#
# rgba(r, g, b, a)  -> pack a color into a 32-bit integer (0.0-1.0 range)
# rgb(r, g, b)      -> same, with alpha = 1.0
# color_pack(r,g,b,a) -> same as rgba (the C-level function)
# color_unpack(c)   -> returns (r, g, b, a) tuple from a packed color
#
state = {"t": 0.0}


def load():
    g.set_title("02 — Color & Gouraud Shading")


def update(dt):
    state["t"] += dt


def draw():
    g.clear((0.02, 0.02, 0.06, 1.0))
    g.set_mode("gouraud")

    # =====================================================================
    # GOURAUD SHADED TRIANGLE
    # =====================================================================
    # Each vertex gets a different color. The GPU linearly interpolates
    # across the triangle surface, creating a smooth rainbow gradient.
    #
    #       red
    #        /\
    #       /  \
    #      /    \
    #     /______\
    #   green    blue
    #
    # This is the default shading model (SHADE_GOURAUD).

    g.triangle(
        (160.0, 30.0,  1.0, 0.2, 0.3),   # top — red/pink
        (50.0,  110.0, 0.2, 1.0, 0.3),    # bottom-left — green
        (270.0, 110.0, 0.2, 0.3, 1.0),    # bottom-right — blue
    )

    # =====================================================================
    # THE rect() HELPER
    # =====================================================================
    # rect(x, y, width, height, color) draws a filled rectangle.
    # Color can be a 3-tuple (r,g,b) or 4-tuple (r,g,b,a).
    # Under the hood it creates 4 vertices and draws 2 triangles.

    # A row of colored squares.
    # Color tuples can be given directly in 0-255 form for convenience.
    colors = [
        (255, 60, 80),    # red
        (255, 160, 40),   # orange
        (255, 240, 60),   # yellow
        (60, 220, 100),   # green
        (60, 160, 255),   # blue
        (180, 80, 255),   # purple
    ]
    for i, c in enumerate(colors):
        x = 28.0 + i * 46.0
        g.rect(x, 128.0, 38.0, 24.0, c)

    # =====================================================================
    # FLAT MODE — constant color
    # =====================================================================
    # set_mode('flat') configures the combine unit to use a single constant
    # color for all vertices, ignoring per-vertex color entirely.
    #
    # The constant color is set with constant_color_value(packed_color).
    # This is useful when you want to draw many shapes the same color
    # without repeating the color on every vertex.

    g.set_mode("flat")
    g.constant_color_value(g.rgba(0.9, 0.7, 0.1, 1.0))

    # These rectangles will all be the same golden color.
    # set_mode('flat') ignores per-vertex color and uses the constant
    # color register instead, so every pixel is identical:
    for i in range(8):
        x = 20.0 + i * 37.0
        y_offset = math.sin(state["t"] * 2.0 + i * 0.8) * 8.0
        g.rect(x, 174.0 + y_offset, 30.0, 14.0)

    # =====================================================================
    # HARDWARE FLAT SHADING — shade_model()
    # =====================================================================
    # set_mode('flat') uses a CONSTANT color register (same color everywhere).
    # shade_model(SHADE_FLAT) is different: it tells the GPU to copy the
    # FIRST vertex's color to ALL vertices in the primitive. The triangle
    # still has 3 different colors in memory, but the GPU flattens them
    # before rasterizing.
    #
    # This is classic "flat shading" — each triangle is a single solid
    # color taken from its first vertex. It looks very different from
    # Gouraud shading where colors are smoothly interpolated.

    g.set_mode("gouraud")
    g.shade_model(g.SHADE_FLAT)

    # Both triangles use the first vertex color for the whole face.
    # The second and third vertex colors are IGNORED by the rasterizer.
    g.triangle(
        (40.0,  160.0, 1.0, 0.2, 0.3),   # first vertex = red/pink (used)
        (100.0, 160.0, 0.2, 1.0, 0.3),   # green (ignored)
        (70.0,  190.0, 0.2, 0.3, 1.0),   # blue (ignored)
    )
    g.triangle(
        (120.0, 160.0, 0.2, 1.0, 0.3),   # first vertex = green (used)
        (180.0, 160.0, 1.0, 0.2, 0.3),   # red (ignored)
        (150.0, 190.0, 0.2, 0.3, 1.0),   # blue (ignored)
    )

    # Restore gouraud shading for the rest of the frame
    g.shade_model(g.SHADE_GOURAUD)

    # =====================================================================
    # ANIMATED GOURAUD QUAD
    # =====================================================================
    # Switch back to gouraud mode for per-vertex color.
    # rect() applies the same color to all 4 corners, so for per-vertex
    # color on a quad we need to build it ourselves with quad().

    g.set_mode("gouraud")

    t = state["t"]
    r0 = 0.5 + 0.5 * math.sin(t * 1.2)
    r1 = 0.5 + 0.5 * math.sin(t * 1.8 + 1.0)
    gr0 = 0.5 + 0.5 * math.sin(t * 0.9 + 2.0)
    gr1 = 0.5 + 0.5 * math.sin(t * 1.5 + 3.0)

    # quad() draws two triangles sharing a diagonal — a filled quadrilateral
    g.quad(
        (40.0,  206.0, r0, 0.2, 0.8),    # top-left
        (280.0, 206.0, r1, 0.8, 0.2),     # top-right
        (280.0, 234.0, 0.2, gr0, 0.9),    # bottom-right
        (40.0,  234.0, 0.9, gr1, 0.2),    # bottom-left
    )
