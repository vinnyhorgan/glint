# =============================================================================
# Tutorial 07 — Depth Buffer & Pseudo-3D
# =============================================================================
#
# What you'll learn:
#   - begin_3d() — enables depth testing for 3D rendering
#   - begin_2d() — disables depth testing for 2D overlays
#   - Vertex z-coordinates for depth ordering
#   - depth_buffer_mode() and depth_buffer_function()
#   - depth_mask() — controlling depth writes
#   - Simple manual 3D transforms (no matrix library needed)
#
# The depth buffer ensures that closer objects overlap farther ones,
# regardless of draw order. Without it, the last-drawn object always wins.
#
# In 2D mode (the default), depth testing is disabled and z is ignored.
# In 3D mode, vertices with smaller z values are "closer to the camera"
# and will occlude vertices with larger z values (when using CMP_LESS).
#
# z range: 0.0 (near) to 1.0 (far)
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0}


def load():
    g.set_title("07 — Depth Buffer & Pseudo-3D")


def update(dt):
    state["t"] += dt


# =============================================================================
# SIMPLE 3D PROJECTION
# =============================================================================
# We don't have a matrix library, so we'll project 3D points to 2D
# manually using a basic perspective formula:
#
#   screen_x = cx + (x - cam_x) * focal / z
#   screen_y = cy + (y - cam_y) * focal / z
#
# This gives us a convincing 3D effect without any matrix math.

def project(x, y, z):
    """Project a 3D point to screen coordinates.

    x, y: position in 3D world space
    z: distance from camera (must be > 0)

    Returns (screen_x, screen_y, depth_z) or None if behind camera.
    """
    if z <= 0.1:
        return None
    focal = 120.0
    sx = 160.0 + x * focal / z
    sy = 120.0 + y * focal / z
    return (sx, sy, z)


# =============================================================================
# DRAW
# =============================================================================

def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))
    g.set_untextured()

    # =====================================================================
    # PART 1: DEPTH DEMO WITHOUT DEPTH BUFFER
    # =====================================================================
    # Draw two overlapping rectangles in 2D mode. The second one always
    # draws on top because there's no depth testing.

    g.begin_2d()

    # Label area
    g.rect(5.0, 5.0, 150.0, 8.0, (0.3, 0.3, 0.3))

    # Red rect drawn first, then blue rect drawn second.
    # Blue overlaps red regardless of z values because depth is disabled.
    g.rect(15.0, 20.0, 55.0, 45.0, (0.9, 0.2, 0.2, 0.8))
    g.rect(40.0, 35.0, 55.0, 45.0, (0.2, 0.2, 0.9, 0.8))

    # =====================================================================
    # PART 2: DEPTH DEMO WITH DEPTH BUFFER
    # =====================================================================
    # Now switch to 3D mode and draw the same rectangles, but this time
    # the red one has z=0.2 (closer) and blue has z=0.6 (farther).
    # With depth testing, red overlaps blue even though blue is drawn later.

    g.begin_3d()

    # Blue rect — farther (z=0.6), drawn FIRST
    v0 = g.vertex(190.0, 35.0, z=0.6, color=(0.2, 0.2, 0.9, 0.8))
    v1 = g.vertex(245.0, 35.0, z=0.6, color=(0.2, 0.2, 0.9, 0.8))
    v2 = g.vertex(245.0, 80.0, z=0.6, color=(0.2, 0.2, 0.9, 0.8))
    v3 = g.vertex(190.0, 80.0, z=0.6, color=(0.2, 0.2, 0.9, 0.8))
    g.quad(v0, v1, v2, v3)

    # Red rect — closer (z=0.2), drawn SECOND
    # Because z=0.2 < z=0.6, the depth buffer keeps this on top.
    v4 = g.vertex(165.0, 50.0, z=0.2, color=(0.9, 0.2, 0.2, 0.8))
    v5 = g.vertex(220.0, 50.0, z=0.2, color=(0.9, 0.2, 0.2, 0.8))
    v6 = g.vertex(220.0, 95.0, z=0.2, color=(0.9, 0.2, 0.2, 0.8))
    v7 = g.vertex(165.0, 95.0, z=0.2, color=(0.9, 0.2, 0.2, 0.8))
    g.quad(v4, v5, v6, v7)

    # =====================================================================
    # PART 3: SIMPLE 3D SCENE — rotating cube-like shape
    # =====================================================================
    # We'll draw a rotating arrangement of colored faces using manual
    # projection. Each face has a different z depth.

    # Define 3D cube vertices (centered at origin)
    size = 0.5
    cube_verts = [
        (-size, -size, -size), (size, -size, -size),
        (size,  size, -size),  (-size, size, -size),
        (-size, -size,  size), (size, -size,  size),
        (size,  size,  size),  (-size, size,  size),
    ]

    # Rotate around Y axis
    angle = t * 0.8
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)
    rotated = []
    for x, y, z in cube_verts:
        rx = x * cos_a - z * sin_a
        rz = x * sin_a + z * cos_a
        ry = y
        rotated.append((rx, ry, rz + 3.0))  # z+3 pushes it in front of camera

    # Project all vertices to screen space
    projected = []
    for x, y, z in rotated:
        p = project(x, y, z)
        if p is None:
            projected.append(None)
        else:
            projected.append(p)

    # Define the 6 faces of the cube (as index quads)
    faces = [
        ([0, 1, 2, 3], (0.9, 0.3, 0.3)),  # front
        ([5, 4, 7, 6], (0.3, 0.9, 0.3)),  # back
        ([4, 0, 3, 7], (0.3, 0.3, 0.9)),  # left
        ([1, 5, 6, 2], (0.9, 0.9, 0.3)),  # right
        ([4, 5, 1, 0], (0.9, 0.3, 0.9)),  # top
        ([3, 2, 6, 7], (0.3, 0.9, 0.9)),  # bottom
    ]

    # Sort faces by average z (painter's algorithm — draw far first)
    # This works together with the depth buffer for correct overlap.
    face_depths = []
    for indices, color in faces:
        avg_z = sum(projected[i][2] for i in indices if projected[i] is not None) / 4.0
        face_depths.append((avg_z, indices, color))
    face_depths.sort(key=lambda f: -f[0])  # far to near

    # Draw each face as two triangles
    for avg_z, indices, color in face_depths:
        if any(projected[i] is None for i in indices):
            continue
        pts = [projected[i] for i in indices]
        z = avg_z * 0.15  # normalize z to 0-1 range for depth buffer
        z = max(0.0, min(1.0, z))
        c = (color[0], color[1], color[2], 1.0)
        g.quad(
            g.vertex(pts[0][0], pts[0][1], z=z, color=c),
            g.vertex(pts[1][0], pts[1][1], z=z, color=c),
            g.vertex(pts[2][0], pts[2][1], z=z, color=c),
            g.vertex(pts[3][0], pts[3][1], z=z, color=c),
        )

    # =====================================================================
    # PART 4: 2D OVERLAY on top of 3D scene
    # =====================================================================
    # Switch back to 2D mode to draw HUD elements on top.
    # begin_2d() disables depth testing so overlays always appear on top.

    g.begin_2d()
    g.set_blend_none()
    g.rect(5.0, 230.0, 310.0, 8.0, (0.15, 0.15, 0.15))
    g.rect(5.0, 230.0, t * 20.0 % 310.0, 8.0, (0.3, 0.6, 1.0))
