# =============================================================================
# Tutorial 03 — Drawing Primitives & Shape Builders
# =============================================================================
#
# What you'll learn:
#   - All primitive types: triangle, quad, rect, line, point
#   - Multiple vertex formats (2-tuple, 5-tuple, Vertex objects)
#   - Building polygons from triangle fans
#   - Drawing circles and other shapes
#   - Organizing drawing code into reusable helpers
#
# Glint provides three primitive draw calls at the hardware level:
#   - draw_triangle(v0, v1, v2)  — filled triangle
#   - draw_line(v0, v1)          — 1-pixel line
#   - draw_point(v)              — 1-pixel point
#
# On top of these, the Python bindings add convenience functions:
#   - triangle(), quad(), rect(), line(), point()
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("03 — Primitives & Shape Builders")


def update(dt):
    state["t"] += dt


# =============================================================================
# SHAPE BUILDERS
# =============================================================================
# The GPU only draws triangles. Everything else (circles, polygons, etc.)
# must be built from triangles. Here are common reusable helpers.

def poly(points, color):
    """Draw a filled convex polygon as a triangle fan.

    'points' is a list of (x, y) tuples.
    'color' is an (r, g, b) or (r, g, b, a) tuple.

    A triangle fan shares the first vertex and walks through pairs of
    subsequent vertices:
        triangle(p0, p1, p2)
        triangle(p0, p2, p3)
        triangle(p0, p3, p4)
        ...
    """
    if len(points) < 3:
        return
    v0 = g.vertex(points[0][0], points[0][1], color=color)
    for i in range(1, len(points) - 1):
        v1 = g.vertex(points[i][0], points[i][1], color=color)
        v2 = g.vertex(points[i + 1][0], points[i + 1][1], color=color)
        g.triangle(v0, v1, v2)


def circle(cx, cy, radius, color, segments=24):
    """Draw a filled circle as a triangle fan.

    More segments = smoother circle. 16-24 is usually enough for
    small circles on a 320x240 screen.
    """
    points = []
    for i in range(segments):
        angle = (i / segments) * math.pi * 2.0
        x = cx + math.cos(angle) * radius
        y = cy + math.sin(angle) * radius
        points.append((x, y))
    poly(points, color)


def ring(cx, cy, inner_r, outer_r, color, segments=24):
    """Draw a ring (annulus) using a quad strip."""
    for i in range(segments):
        a0 = (i / segments) * math.pi * 2.0
        a1 = ((i + 1) / segments) * math.pi * 2.0
        p0 = (cx + math.cos(a0) * outer_r, cy + math.sin(a0) * outer_r)
        p1 = (cx + math.cos(a1) * outer_r, cy + math.sin(a1) * outer_r)
        p2 = (cx + math.cos(a1) * inner_r, cy + math.sin(a1) * inner_r)
        p3 = (cx + math.cos(a0) * inner_r, cy + math.sin(a0) * inner_r)
        g.quad(
            g.vertex(p0[0], p0[1], color=color),
            g.vertex(p1[0], p1[1], color=color),
            g.vertex(p2[0], p2[1], color=color),
            g.vertex(p3[0], p3[1], color=color),
        )


# =============================================================================
# DRAW
# =============================================================================

def draw():
    t = state["t"]
    g.clear((0.03, 0.03, 0.08, 1.0))
    g.set_untextured()

    # -- DIVIDER LINES --------------------------------------------------------
    # line() takes two vertices and draws a 1-pixel-wide line.
    # The 5-tuple form (x, y, r, g, b) gives position + color.

    g.line(
        (10.0, 10.0, 0.4, 0.6, 1.0),
        (150.0, 10.0, 0.4, 0.6, 1.0),
    )
    g.line(
        (170.0, 10.0, 1.0, 0.8, 0.3),
        (310.0, 10.0, 1.0, 0.8, 0.3),
    )
    g.line(
        (10.0, 100.0, 0.3, 1.0, 0.6),
        (150.0, 100.0, 0.3, 1.0, 0.6),
    )
    g.line(
        (170.0, 100.0, 1.0, 0.4, 0.7),
        (310.0, 100.0, 1.0, 0.4, 0.7),
    )
    g.line(
        (10.0, 185.0, 0.5, 0.5, 1.0),
        (150.0, 185.0, 0.5, 0.5, 1.0),
    )
    g.line(
        (170.0, 185.0, 1.0, 0.9, 0.2),
        (310.0, 185.0, 1.0, 0.9, 0.2),
    )

    # -- TRIANGLES (top-left) -------------------------------------------------
    # Simple colored triangle with the 5-tuple vertex format (x, y, r, g, b)
    g.triangle(
        (30.0,  35.0, 1.0, 0.3, 0.3),   # red vertex
        (30.0,  90.0, 0.3, 1.0, 0.3),   # green vertex
        (120.0, 62.0, 0.3, 0.3, 1.0),   # blue vertex
    )

    # -- QUADS (top-right) ----------------------------------------------------
    # quad() draws two triangles forming a quadrilateral.
    # gouraud shading blends the 4 corner colors across the surface.
    g.quad(
        (175.0, 30.0, 1.0, 0.5, 0.1),
        (305.0, 30.0, 1.0, 0.8, 0.1),
        (305.0, 90.0, 1.0, 0.8, 0.4),
        (175.0, 90.0, 1.0, 0.5, 0.4),
    )

    # -- RECTS (second row, left) ---------------------------------------------
    # rect(x, y, w, h, color) is the quickest way to draw a filled rectangle.
    for i in range(5):
        x = 15.0 + i * 28.0
        brightness = 0.3 + 0.14 * i
        g.rect(x, 110.0, 22.0, 30.0, (brightness, 1.0, 0.5))

    # -- POINTS (second row, right) -------------------------------------------
    # point(v) draws a single pixel. We'll draw a spiral pattern.
    for i in range(120):
        angle = i * 0.25
        radius = 3.0 + i * 0.25
        x = 240.0 + math.cos(angle) * radius
        y = 140.0 + math.sin(angle) * radius
        c = (0.9, 0.4 + 0.004 * i, 0.7)
        g.point((x, y, c[0], c[1], c[2]))

    # -- CIRCLE (bottom-left, animated) ---------------------------------------
    r = 20.0 + math.sin(t * 2.5) * 6.0
    circle(80.0, 215.0, r, (0.4, 0.5, 1.0, 1.0), 32)

    # -- ROTATING POLYGON (bottom-right) --------------------------------------
    angle_offset = t * 0.8
    pts = []
    for i in range(10):
        a = (i / 10.0) * math.pi * 2.0 - math.pi / 2.0 + angle_offset
        r = 24.0 if i % 2 == 0 else 10.0
        pts.append((240.0 + math.cos(a) * r, 215.0 + math.sin(a) * r))
    poly(pts, (1.0, 0.9, 0.2))

    # -- RING (decorative outline) --------------------------------------------
    ring(240.0, 215.0, 26.0, 29.0, (0.8, 0.7, 0.1), 32)
