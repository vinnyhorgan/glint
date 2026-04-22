# =============================================================================
# Tutorial 09 — Fog
# =============================================================================
#
# What you'll learn:
#   - fog_mode() — table fog, iterated-alpha fog, disable
#   - fog_color_value() — what color the fog fades to
#   - fog_table() — 64-entry density table for depth-based fog
#   - make_fog_table() — convenience function to generate a fog table
#   - Creating atmospheric depth effects
#   - Combining fog with 3D rendering
#
# Fog blends pixel color toward a fog color based on distance from the
# camera. It's computed per-pixel in the fragment shader:
#
#   final_color = fog_color * fog_factor + pixel_color * (1 - fog_factor)
#
# There are two fog modes:
#
#   FOG_WITH_TABLE — fog factor comes from a 64-entry lookup table
#     indexed by the vertex's depth (z/w). This gives precise control
#     over the fog curve.
#
#   FOG_WITH_ITERATED_ALPHA — fog factor = vertex alpha.
#     A simple shortcut: set alpha=0 for no fog, alpha=1 for full fog.
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0, "fog_color": None}


def load():
    g.set_title("09 — Fog")

    # =====================================================================
    # FOG COLOR
    # =====================================================================
    # fog_color_value() sets the color that pixels fade toward.
    # It takes a packed color (use rgba() or rgb() to create one).

    state["fog_color"] = g.rgb(0.04, 0.04, 0.10)  # dark blue-black


def update(dt):
    state["t"] += dt


# =============================================================================
# FOG TABLE GENERATOR
# =============================================================================
# make_fog_table(start_w, end_w) creates a 64-entry fog table where:
#   - At depth < start_w, there's no fog (fog_factor = 0)
#   - At depth > end_w, there's full fog (fog_factor = 1)
#   - Between start_w and end_w, fog increases linearly
#
# The table maps depth values to fog density (0-255).
# The depth values follow a non-linear distribution matching the Glide
# hardware: w = 2^(3 + (i>>2)) / (8 - (i&3))


def draw():
    t = state["t"]
    g.begin_3d()
    g.clear((0.04, 0.04, 0.10, 1.0))
    g.set_untextured()

    # =====================================================================
    # PART 1: TABLE FOG — depth-based atmospheric fog
    # =====================================================================
    # Enable depth buffer for 3D mode, then turn on table fog.

    g.begin_3d()
    g.fog_color_value(state["fog_color"])

    # Generate a fog table where fog starts at w=1.0 and becomes full at w=8.0
    fog = g.make_fog_table(1.0, 8.0)
    g.fog_table(fog)
    g.fog_mode(g.FOG_WITH_TABLE)

    # Draw a row of rectangles at increasing z values.
    # Closer ones are bright; farther ones fade into the fog color.
    for i in range(10):
        z = 0.1 + i * 0.08
        x = 10.0 + i * 30.0
        # The fog factor depends on the vertex's depth (z/oow).
        # As z increases, fog_factor increases → color → fog_color.
        c = (0.4, 0.7, 1.0, 1.0)
        g.rect(x, 10.0, 26.0, 60.0, c, z=z)

    # Disable fog for the next section
    g.fog_mode(g.FOG_DISABLE)

    # =====================================================================
    # PART 2: ANIMATED FOG DISTANCE
    # =====================================================================
    # By changing the fog table parameters over time, we can create
    # fog that rolls in and out.

    fog_near = 1.0 + math.sin(t * 0.5) * 0.5
    fog_far = 5.0 + math.sin(t * 0.3) * 2.0
    fog = g.make_fog_table(fog_near, fog_far)
    g.fog_table(fog)
    g.fog_mode(g.FOG_WITH_TABLE)

    # A grid of colored blocks receding into fog
    for row in range(4):
        for col in range(10):
            z = 0.15 + col * 0.08
            x = 15.0 + col * 30.0
            y = 85.0 + row * 25.0
            r = 0.3 + row * 0.15
            gr = 0.5 + col * 0.04
            b = 0.8 - row * 0.1
            g.rect(x, y, 24.0, 18.0, (r, gr, b, 1.0), z=z)

    g.fog_mode(g.FOG_DISABLE)
    g.begin_2d()

    # =====================================================================
    # PART 3: ITERATED ALPHA FOG — using vertex alpha as fog
    # =====================================================================
    # With FOG_WITH_ITERATED_ALPHA, the vertex's alpha component controls
    # how much fog is applied. alpha=0 means no fog, alpha=1 means full fog.
    # This is a simple way to add fog without a fog table.

    g.begin_3d()
    g.fog_color_value(g.rgb(0.04, 0.04, 0.10))
    g.fog_mode(g.FOG_WITH_ITERATED_ALPHA)

    # Each row has increasing alpha (more fog)
    for i in range(10):
        x = 10.0 + i * 30.0
        alpha = i / 9.0  # 0.0 (no fog) to 1.0 (full fog)
        g.rect(x, 195.0, 26.0, 30.0, (0.6, 0.8, 1.0, alpha))

    g.fog_mode(g.FOG_DISABLE)
    g.begin_2d()

    # =====================================================================
    # PART 4: FOG WITH DEPTH-COLORED LANDSCAPE
    # =====================================================================
    # A simple mountain scene with fog in the distance to demonstrate
    # atmospheric perspective — objects farther away are hazier.

    g.begin_3d()
    g.fog_color_value(g.rgb(0.06, 0.05, 0.12))
    fog = g.make_fog_table(0.5, 4.0)
    g.fog_table(fog)
    g.fog_mode(g.FOG_WITH_TABLE)

    # Ground plane — a series of horizontal strips at increasing depth
    for i in range(12):
        z = 0.05 + i * 0.08
        y = 170.0 + i * 5.0
        brightness = 0.15 + (1.0 - i / 12.0) * 0.3
        g.rect(0.0, y, 320.0, 6.0, (brightness, brightness * 0.8, brightness * 0.5, 1.0), z=z)

    # Mountains at various depths
    mountains = [
        (0.1,  (-20.0, 200.0), (160.0, 130.0), (340.0, 200.0)),
        (0.3,  (-10.0, 200.0), (100.0, 145.0), (220.0, 200.0)),
        (0.5,  (80.0,  200.0), (200.0, 140.0), (330.0, 200.0)),
        (0.7,  (-30.0, 200.0), (60.0,  155.0), (160.0, 200.0)),
    ]
    for z, p0, p1, p2 in mountains:
        g.triangle(
            g.vertex(p0[0], p0[1], z=z, color=(0.2, 0.15, 0.3, 1.0)),
            g.vertex(p1[0], p1[1], z=z, color=(0.25, 0.18, 0.35, 1.0)),
            g.vertex(p2[0], p2[1], z=z, color=(0.2, 0.15, 0.3, 1.0)),
        )

    g.fog_mode(g.FOG_DISABLE)
    g.begin_2d()
