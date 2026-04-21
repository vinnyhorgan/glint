# =============================================================================
# Tutorial 11 — Alpha Testing
# =============================================================================
#
# What you'll learn:
#   - alpha_test_function() — the 8 comparison functions
#   - alpha_test_reference_value() — the threshold to compare against
#   - Difference between alpha TEST and alpha BLEND
#   - Creating cutout / decal effects
#   - Dithered transparency patterns
#   - Combining alpha test with blending
#
# Alpha TESTING is different from alpha BLENDING:
#
#   Alpha BLENDING combines the new pixel with the existing framebuffer
#   pixel based on alpha. Partially transparent pixels create smooth fades.
#
#   Alpha TESTING compares the incoming pixel's alpha against a reference
#   value using a comparison function. Pixels that fail are DISCARDED
#   entirely (not drawn at all). This creates hard-edged cutouts.
#
# The 8 comparison functions (same as depth):
#   CMP_NEVER, CMP_LESS, CMP_EQUAL, CMP_LEQUAL,
#   CMP_GREATER, CMP_NOTEQUAL, CMP_GEQUAL, CMP_ALWAYS
#
# Default: CMP_ALWAYS (everything passes — no testing)
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("11 — Alpha Testing")


def update(dt):
    state["t"] += dt


def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))
    g.set_untextured()

    # =====================================================================
    # PART 1: ALPHA TEST VS ALPHA BLEND — side by side
    # =====================================================================
    # Left: alpha blending — smooth transparency
    # Right: alpha test — hard cutout

    # Background: a checkerboard pattern so we can see transparency
    for y in range(8):
        for x in range(20):
            c = (0.1, 0.1, 0.15) if (x + y) % 2 == 0 else (0.08, 0.08, 0.12)
            g.rect(x * 16.0, y * 8.0, 16.0, 8.0, c)

    # -- Left side: alpha blending (smooth fade) --
    # No alpha test (ALWAYS passes), with alpha blending
    g.alpha_test_function(g.CMP_ALWAYS)
    g.set_blend_alpha()

    for i in range(16):
        x = 5.0 + i * 9.0
        alpha = i / 15.0
        g.rect(x, 70.0, 8.0, 40.0, (0.3, 0.6, 1.0, alpha))

    g.set_blend_none()

    # -- Right side: alpha test (hard cutout) --
    # Set reference value to 128 (0.5 in 0-255 range)
    # CMP_GREATER: only draw pixels with alpha > 128
    g.alpha_test_function(g.CMP_GREATER)
    g.alpha_test_reference_value(128)

    for i in range(16):
        x = 170.0 + i * 9.0
        alpha = i / 15.0
        # This rect will only appear where alpha > 0.5 (last 8 rects)
        g.rect(x, 70.0, 8.0, 40.0, (1.0, 0.5, 0.3, alpha))

    # Reset alpha test
    g.alpha_test_function(g.CMP_ALWAYS)

    # =====================================================================
    # PART 2: DITHERED TRANSPARENCY (classic PS1-style)
    # =====================================================================
    # By using a checkerboard pattern of alpha 0 and 255, combined with
    # CMP_NOTEQUAL and reference=0, we can create a dithered transparency
    # effect. This was commonly used on PS1 and other early 3D hardware.

    g.alpha_test_function(g.CMP_NOTEQUAL)
    g.alpha_test_reference_value(0)

    cell = 4
    for y in range(30):
        for x in range(40):
            # Checkerboard: every other cell gets alpha=255
            if (x + y) % 2 == 0:
                continue
            px = 20.0 + x * cell
            py = 120.0 + y * cell
            # Only draw the "on" pixels — creates 50% dithered fill
            g.rect(px, py, cell, cell, (0.8, 0.4, 0.9))

    g.alpha_test_function(g.CMP_ALWAYS)

    # =====================================================================
    # PART 3: ANIMATED ALPHA TEST THRESHOLD
    # =====================================================================
    # We can animate the reference value to create a "dissolve" effect.
    # Pixels gradually disappear as the threshold rises.

    # Draw a large gradient rectangle
    g.alpha_test_function(g.CMP_GREATER)

    # Animate the threshold from 0 to 255 and back
    threshold = int(128.0 + math.sin(t * 1.5) * 127.0)
    g.alpha_test_reference_value(threshold)

    for y in range(12):
        for x in range(20):
            alpha = int((x + y) / 32.0 * 255.0)
            px = 0.0 + x * 16.0
            py = 120.0 + y * 10.0
            g.rect(px, py, 16.0, 10.0, (0.3, 0.7, 1.0, alpha / 255.0))

    g.alpha_test_function(g.CMP_ALWAYS)

    # =====================================================================
    # PART 4: ALL 8 COMPARISON FUNCTIONS
    # =====================================================================
    # A visual reference showing what each function does.
    # Each bar has alpha going from 0 to 255 left-to-right.
    # Pixels where the test fails are not drawn (you see the checkerboard
    # background showing through).

    funcs = [
        (g.CMP_NEVER,    "NEVER  "),
        (g.CMP_LESS,     "LESS   "),
        (g.CMP_EQUAL,    "EQUAL  "),
        (g.CMP_LEQUAL,   "LEQUAL "),
        (g.CMP_GREATER,  "GREATER"),
        (g.CMP_NOTEQUAL, "NOTEQ  "),
        (g.CMP_GEQUAL,   "GEQUAL "),
        (g.CMP_ALWAYS,   "ALWAYS "),
    ]

    g.set_blend_none()
    g.alpha_test_reference_value(128)

    for i, (func, label) in enumerate(funcs):
        y_base = 4.0 + i * 3.0
        g.alpha_test_function(func)
        # Draw a thin bar with alpha gradient from 0 (left) to 255 (right)
        for j in range(32):
            alpha = int((j / 31.0) * 255.0)
            px = 5.0 + j * 4.5
            g.rect(px, y_base, 4.0, 2.5, (0.3, 0.7, 1.0, alpha / 255.0))

    g.alpha_test_function(g.CMP_ALWAYS)

    # =====================================================================
    # PART 5: COMBINED ALPHA TEST + BLEND
    # =====================================================================
    # Alpha test happens BEFORE blending. So we can:
    #   1. Use alpha test to discard fully transparent pixels
    #   2. Use alpha blending to smooth the remaining semi-transparent edges
    #
    # This is the standard approach for rendering textured sprites with
    # soft anti-aliased edges — the test removes the "halo" artifacts.

    # Discard pixels with alpha < 32 (very transparent)
    g.alpha_test_function(g.CMP_GREATER)
    g.alpha_test_reference_value(32)
    g.set_blend_alpha()

    # Draw overlapping semi-transparent circles
    for i in range(5):
        phase = t * 0.7 + i * 1.3
        cx = 260.0 + math.sin(phase) * 30.0
        cy = 150.0 + math.cos(phase * 0.9) * 40.0
        alpha = 0.3 + 0.3 * math.sin(t + i)
        color = [
            (1.0, 0.3, 0.5, alpha),
            (0.3, 1.0, 0.5, alpha),
            (0.5, 0.3, 1.0, alpha),
            (1.0, 1.0, 0.3, alpha),
            (0.3, 1.0, 1.0, alpha),
        ][i]

        # Circle as triangle fan
        segments = 16
        for j in range(segments):
            a0 = (j / segments) * math.pi * 2.0
            a1 = ((j + 1) / segments) * math.pi * 2.0
            r = 18.0
            g.triangle(
                g.vertex(cx, cy, color=color),
                g.vertex(cx + math.cos(a0) * r, cy + math.sin(a0) * r, color=color),
                g.vertex(cx + math.cos(a1) * r, cy + math.sin(a1) * r, color=color),
            )

    # Reset
    g.alpha_test_function(g.CMP_ALWAYS)
    g.set_blend_none()
