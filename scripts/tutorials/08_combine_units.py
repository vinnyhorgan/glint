# =============================================================================
# Tutorial 08 — The Combine Unit (Advanced)
# =============================================================================
#
# What you'll learn:
#   - How color_combine() and alpha_combine() work
#   - The 11 combine functions
#   - The 11 combine factors
#   - The 4 local sources and 4 other sources
#   - How set_untextured() / set_textured_modulate() / set_mode() work
#   - Creating custom blend effects manually
#   - The invert flag
#
# The combine unit is the heart of the Glide rendering pipeline. It
# determines how the final color of each pixel is computed before blending.
#
# The formula:
#   result = function(local, factor * other)    (optionally inverted)
#
# Where:
#   local  = one of: ITERATED (vertex color), CONSTANT, DEPTH, NONE (0)
#   other  = one of: ITERATED, TEXTURE, CONSTANT, NONE (0)
#   factor = scales 'other' before combining
#   function = how to merge local and scaled-other
#
# This is essentially a configurable blending math unit. The convenience
# functions you've been using are just presets for the combine unit.
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("08 — The Combine Unit")


def update(dt):
    state["t"] += dt


def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))

    # =====================================================================
    # WHAT set_untextured() ACTUALLY DOES
    # =====================================================================
    # When you call set_untextured(), it configures:
    #
    #   color_combine(
    #       COMBINE_FUNCTION_LOCAL,       # output = local (vertex color)
    #       COMBINE_FACTOR_NONE,           # factor = ignored
    #       COMBINE_LOCAL_ITERATED,        # local = per-vertex color
    #       COMBINE_OTHER_NONE,            # other = ignored
    #       False)                         # no invert
    #
    # This means: "just pass through the vertex color unchanged."
    #
    # Let's replicate it manually:

    g.color_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        False,
    )
    g.alpha_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        False,
    )
    g.set_blend_none()

    # This draws exactly the same as if we called set_untextured():
    g.rect(10.0, 10.0, 60.0, 30.0, (0.3, 0.8, 0.4))

    # =====================================================================
    # CONSTANT COLOR — using the constant color register
    # =====================================================================
    # Instead of per-vertex color, we can use a global constant color.
    # This is what set_mode('flat') does.

    g.constant_color_value(g.rgba(1.0, 0.5, 0.1, 1.0))
    g.color_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_CONSTANT,   # local = constant color
        g.COMBINE_OTHER_NONE,
        False,
    )

    g.rect(80.0, 10.0, 60.0, 30.0)  # no vertex color needed

    # =====================================================================
    # SCALE_OTHER — tinting with vertex color
    # =====================================================================
    # COMBINE_FUNCTION_SCALE_OTHER outputs: factor * other
    # We can use this to tint a constant color by the vertex brightness.

    g.color_combine(
        g.COMBINE_FUNCTION_SCALE_OTHER,     # output = factor * other
        g.COMBINE_FACTOR_LOCAL,              # factor = vertex color
        g.COMBINE_LOCAL_ITERATED,            # local = vertex color (used as factor)
        g.COMBINE_OTHER_CONSTANT,            # other = constant color
        False,
    )
    g.constant_color_value(g.rgba(1.0, 0.3, 0.1, 1.0))  # orange constant

    # Each rect scales the orange by its vertex brightness
    for i in range(8):
        brightness = 0.1 + i * 0.12
        x = 10.0 + i * 37.0
        g.rect(x, 50.0, 33.0, 20.0, (brightness, brightness, brightness))

    # =====================================================================
    # INVERT — producing (1 - color)
    # =====================================================================
    # The invert flag flips the final result: output = 1.0 - output
    # This creates a color inversion effect.

    g.color_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        True,  # INVERT — output = 1.0 - vertex_color
    )

    # These rectangles show inverted colors:
    # Red vertex → cyan output, Green → magenta, Blue → yellow
    g.triangle(
        (80.0,  80.0, 1.0, 0.0, 0.0),  # red -> cyan
        (20.0,  140.0, 0.0, 1.0, 0.0),  # green -> magenta
        (140.0, 140.0, 0.0, 0.0, 1.0),  # blue -> yellow
    )

    # =====================================================================
    # SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL — highlight blending
    # =====================================================================
    # COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL computes:
    #   factor * (other - local) + local
    # With factor=1 this simplifies to 'other', but with partial factors
    # it creates highlight and emboss-like effects.

    g.color_combine(
        g.COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL,
        g.COMBINE_FACTOR_ONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_CONSTANT,
        False,
    )
    g.constant_color_value(g.rgba(0.6, 0.8, 1.0, 1.0))

    # Bright highlight on top of vertex colors
    for i in range(4):
        x = 160.0 + i * 40.0
        b = 0.15 + i * 0.15
        g.rect(x, 80.0, 35.0, 30.0, (b, b * 0.5, b * 0.3))

    # =====================================================================
    # THE 11 COMBINE FUNCTIONS
    # =====================================================================
    # Here's the full list of what 'function' can be:
    #
    #   ZERO                         → always 0
    #   LOCAL                        → local color
    #   LOCAL_ALPHA                  → local.alpha (grey)
    #   SCALE_OTHER                  → factor * other
    #   SCALE_OTHER_ADD_LOCAL        → factor * other + local
    #   SCALE_OTHER_ADD_LOCAL_ALPHA  → factor * other + local.alpha
    #   SCALE_OTHER_MINUS_LOCAL      → factor * (other - local)
    #   SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL       → factor*(other-local) + local
    #   SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA → factor*(other-local) + local.alpha
    #   SCALE_MINUS_LOCAL_ADD_LOCAL            → factor*(-local) + local
    #   SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA      → factor*(-local) + local.alpha

    # Reset to normal for a demo of LOCAL_ALPHA (grayscale from alpha channel)
    g.color_combine(
        g.COMBINE_FUNCTION_LOCAL_ALPHA,  # use vertex alpha as brightness
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        False,
    )
    g.alpha_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        False,
    )

    # Draw rects with varying alpha — displayed as brightness
    for i in range(10):
        x = 10.0 + i * 30.0
        a = i / 9.0
        g.rect(x, 150.0, 26.0, 30.0, (1.0, 1.0, 1.0, a))

    # =====================================================================
    # COMBINE_LOCAL_DEPTH — using depth as a color source
    # =====================================================================
    # DEPTH as local source produces clamp(1.0 - z, 0.0, 1.0), giving
    # a brightness that decreases with distance. This creates automatic
    # depth-based shading.

    g.begin_3d()

    g.color_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_DEPTH,   # brightness based on z value
        g.COMBINE_OTHER_NONE,
        False,
    )
    g.alpha_combine(
        g.COMBINE_FUNCTION_LOCAL,
        g.COMBINE_FACTOR_NONE,
        g.COMBINE_LOCAL_ITERATED,
        g.COMBINE_OTHER_NONE,
        False,
    )

    # A row of rects at increasing z values — farther = darker
    for i in range(8):
        z = 0.1 + i * 0.1
        x = 10.0 + i * 38.0
        g.rect(x, 195.0, 34.0, 35.0, (1.0, 1.0, 1.0, 1.0), z=z)

    g.begin_2d()
