# =============================================================================
# Tutorial 05 — Alpha Blending
# =============================================================================
#
# What you'll learn:
#   - set_blend_alpha() — standard transparency
#   - set_blend_add() — additive glow / light effects
#   - set_blend_none() — opaque (default)
#   - Per-vertex alpha via the Vertex class
#   - How alpha affects layering and compositing
#   - Using blend modes creatively
#
# Alpha blending determines how new pixels combine with what's already in
# the framebuffer. Glint supports several blend modes via the Glide API.
#
# The three convenience functions cover most cases:
#
#   set_blend_none()   — src*1 + dst*0  (overwrite, no blending)
#   set_blend_alpha()  — src*a + dst*(1-a)  (standard transparency)
#   set_blend_add()    — src*a + dst*1  (additive, for glowing effects)
#
# For full control, alpha_blend_function(rgb_sf, rgb_df, alpha_sf, alpha_df)
# lets you specify separate blend factors for RGB and alpha channels.
#
# Available blend factors:
#   BLEND_ZERO, BLEND_ONE,
#   BLEND_SRC_COLOR, BLEND_ONE_MINUS_SRC_COLOR,
#   BLEND_DST_COLOR, BLEND_ONE_MINUS_DST_COLOR,
#   BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA,
#   BLEND_DST_ALPHA, BLEND_ONE_MINUS_DST_ALPHA,
#   BLEND_ALPHA_SATURATE
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("05 — Alpha Blending")


def update(dt):
    state["t"] += dt


def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))
    g.set_mode("gouraud")

    # =====================================================================
    # SECTION 1: OPAQUE BACKGROUND SHAPES
    # =====================================================================
    # Draw some opaque shapes first as a backdrop for blending demos.

    g.set_blend_none()
    g.rect(20.0, 15.0, 100.0, 60.0, (0.15, 0.15, 0.3))
    g.rect(200.0, 15.0, 100.0, 60.0, (0.15, 0.15, 0.3))
    g.rect(20.0, 90.0, 280.0, 60.0, (0.12, 0.12, 0.25))

    # =====================================================================
    # SECTION 2: ALPHA BLENDING (standard transparency)
    # =====================================================================
    # With alpha blending, a vertex with alpha=0.5 will blend 50/50 with
    # what's behind it. This is the most common blending mode.
    #
    # formula: result = src_color * src_alpha + dst_color * (1 - src_alpha)

    g.set_blend_alpha()

    # Semi-transparent colored rectangles overlapping each other
    g.rect(25.0, 20.0, 50.0, 50.0, (1.0, 0.2, 0.2, 0.6))  # red
    g.rect(55.0, 20.0, 50.0, 50.0, (0.2, 1.0, 0.2, 0.6))  # green
    g.rect(40.0, 35.0, 50.0, 50.0, (0.2, 0.2, 1.0, 0.6))  # blue

    # =====================================================================
    # SECTION 3: ADDITIVE BLENDING (glow / light)
    # =====================================================================
    # Additive blending adds the source color on top of the destination.
    # This makes things look like they glow or emit light. Bright areas
    # stack and get brighter.
    #
    # formula: result = src_color * src_alpha + dst_color * 1

    g.set_blend_add()

    # Glowing circles that overlap and intensify
    for i in range(3):
        phase = t * 1.5 + i * 2.1
        cx = 230.0 + math.sin(phase) * 25.0
        cy = 45.0 + math.cos(phase * 0.7) * 12.0
        alpha = 0.25 + 0.1 * math.sin(t * 3.0 + i)
        color = [
            (0.4, 0.1, 1.0, alpha),  # blue glow
            (1.0, 0.2, 0.5, alpha),  # pink glow
            (0.1, 1.0, 0.4, alpha),  # green glow
        ][i]
        # Draw multiple overlapping circles for a soft glow effect.
        # line() only gets alpha if we pass full Vertex objects here.
        for layer in range(4):
            r = 8.0 + layer * 6.0
            a = color[3] * (1.0 - layer * 0.2)
            segments = 20
            for j in range(segments):
                a0 = (j / segments) * math.pi * 2.0
                a1 = ((j + 1) / segments) * math.pi * 2.0
                g.line(
                    g.vertex(
                        cx + math.cos(a0) * r,
                        cy + math.sin(a0) * r,
                        color=(color[0], color[1], color[2], a),
                    ),
                    g.vertex(
                        cx + math.cos(a1) * r,
                        cy + math.sin(a1) * r,
                        color=(color[0], color[1], color[2], a),
                    ),
                )

    # =====================================================================
    # SECTION 4: ALPHA GRADIENT STRIP
    # =====================================================================
    # A row of rectangles with alpha going from 0.0 to 1.0.
    # This demonstrates how alpha blending affects visibility.

    g.set_blend_alpha()
    for i in range(16):
        x = 25.0 + i * 17.0
        alpha = i / 15.0
        g.rect(x, 95.0, 14.0, 40.0, (0.3, 0.6, 1.0, alpha))

    # =====================================================================
    # SECTION 5: LAYERED TRANSPARENCY
    # =====================================================================
    # Multiple semi-transparent layers create depth. The order matters:
    # things drawn first are "behind" things drawn later.

    g.set_blend_alpha()

    # Three animated circles with different alphas, overlapping
    for layer in range(3):
        phase = t * (0.6 + layer * 0.3) + layer * 1.0
        cx = 80.0 + layer * 70.0 + math.sin(phase) * 20.0
        cy = 190.0 + math.cos(phase * 0.8) * 10.0
        alpha = 0.35 + layer * 0.1
        colors = [
            (1.0, 0.3, 0.3, alpha),
            (0.3, 1.0, 0.5, alpha),
            (0.3, 0.5, 1.0, alpha),
        ]
        c = colors[layer]
        # Simple circle
        segments = 24
        for i in range(segments):
            a0 = (i / segments) * math.pi * 2.0
            a1 = ((i + 1) / segments) * math.pi * 2.0
            r = 25.0 + layer * 5.0
            pts = [
                (cx, cy),
                (cx + math.cos(a0) * r, cy + math.sin(a0) * r),
                (cx + math.cos(a1) * r, cy + math.sin(a1) * r),
            ]
            g.triangle(
                g.vertex(pts[0][0], pts[0][1], color=c),
                g.vertex(pts[1][0], pts[1][1], color=c),
                g.vertex(pts[2][0], pts[2][1], color=c),
            )

    # =====================================================================
    # SECTION 6: CUSTOM BLEND FUNCTIONS
    # =====================================================================
    # The three set_blend_*() helpers are just presets. You can build
    # your own effects with alpha_blend_function() using the 11 blend
    # factors. Here are two useful custom modes:
    #
    # MULTIPLY (shadow / darken):
    #   src_factor = BLEND_DST_COLOR  → multiply source by framebuffer
    #   dst_factor = BLEND_ZERO       → ignore destination
    #   result = src_color * dst_color
    #   White source = no change. Black source = black. Gray = darker.
    #
    # SCREEN (highlight / lighten):
    #   src_factor = BLEND_ONE
    #   dst_factor = BLEND_ONE_MINUS_SRC_COLOR
    #   result = src + dst * (1 - src_color)
    #   Black source = no change. White source = white. Gray = lighter.
    #
    # We wrap this section in push_state() / pop_state() so the custom
    # blend settings are automatically restored afterward. This keeps
    # each section self-contained and prevents state leaks.

    g.push_state()

    # -- Multiply shadow demo --
    g.alpha_blend_function(
        g.BLEND_DST_COLOR, g.BLEND_ZERO,
        g.BLEND_ONE, g.BLEND_ZERO,
    )
    # A dark blue overlay that deepens whatever is beneath it
    g.rect(200.0, 95.0, 100.0, 40.0, (0.4, 0.4, 0.7, 1.0))

    # -- Screen highlight demo --
    g.alpha_blend_function(
        g.BLEND_ONE, g.BLEND_ONE_MINUS_SRC_COLOR,
        g.BLEND_ONE, g.BLEND_ZERO,
    )
    # A bright yellow overlay that lightens whatever is beneath it
    g.rect(215.0, 105.0, 70.0, 20.0, (0.6, 0.6, 0.2, 1.0))

    g.pop_state()

    # =====================================================================
    # SECTION 7: ADDITIVE GLOW OVERLAY
    # =====================================================================
    # A bright additive glow at the bottom ties the scene together.
    # Because Section 6 used push_state/pop_state, we're back to the
    # alpha blending mode from Section 5 here. We switch to additive
    # just for this glow, then pop it back.

    g.push_state()
    g.set_blend_add()
    glow_alpha = 0.15 + 0.05 * math.sin(t * 2.0)
    g.rect(0.0, 225.0, 320.0, 15.0, (0.3, 0.1, 0.5, glow_alpha))
    g.pop_state()
