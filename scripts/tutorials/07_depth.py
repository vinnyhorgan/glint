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
    g.set_mode("gouraud")
    g.set_blend_none()

    # =====================================================================
    # PART 1: DEPTH DEMO WITHOUT DEPTH BUFFER
    # =====================================================================
    # Draw two overlapping rectangles in 2D mode. The second one always
    # draws on top because there's no depth testing.

    g.begin_2d()

    # Left panel: no depth testing.
    g.rect(10.0, 10.0, 135.0, 75.0, (0.08, 0.08, 0.12))

    # Red rect drawn first, then blue rect drawn second.
    # Blue overlaps red regardless of z values because depth is disabled.
    g.rect(20.0, 20.0, 55.0, 45.0, (0.9, 0.2, 0.2, 1.0))
    g.rect(45.0, 35.0, 55.0, 45.0, (0.2, 0.2, 0.9, 1.0))

    # =====================================================================
    # PART 2: DEPTH DEMO WITH DEPTH BUFFER
    # =====================================================================
    # Now switch to 3D mode and draw the same rectangles, but this time
    # the red one has z=0.2 (closer) and blue has z=0.6 (farther).
    # With depth testing, red overlaps blue even though blue is drawn later.

    g.begin_3d()

    # Right panel: same overlap, but depth-tested.
    g.begin_2d()
    g.rect(175.0, 10.0, 135.0, 75.0, (0.08, 0.08, 0.12))
    g.begin_3d()

    # Red rect — closer (z=0.2), drawn FIRST.
    g.rect(185.0, 20.0, 55.0, 45.0, (0.9, 0.2, 0.2, 1.0), z=0.2)

    # Blue rect — farther (z=0.6), drawn SECOND.
    # Even though it's drawn later, it stays behind because 0.6 > 0.2.
    g.rect(210.0, 35.0, 55.0, 45.0, (0.2, 0.2, 0.9, 1.0), z=0.6)

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
        ([0, 1, 2, 3], (0.95, 0.35, 0.35)),
        ([5, 4, 7, 6], (0.25, 0.75, 0.35)),
        ([4, 0, 3, 7], (0.25, 0.45, 1.0)),
        ([1, 5, 6, 2], (1.0, 0.9, 0.25)),
        ([4, 5, 1, 0], (1.0, 0.45, 0.85)),
        ([3, 2, 6, 7], (0.35, 0.95, 0.95)),
    ]

    # Sort faces by average z (painter's algorithm — draw far first).
    # We still keep each vertex's own depth so the depth buffer can do
    # proper per-pixel occlusion across overlapping faces.
    face_depths = []
    for indices, color in faces:
        total = 0.0
        for i in indices:
            if projected[i] is not None:
                total += projected[i][2]
        avg_z = total / 4.0
        face_depths.append((avg_z, indices, color))

    # Bubble sort by avg_z descending (far to near)
    n = len(face_depths)
    for i in range(n):
        for j in range(0, n - i - 1):
            if face_depths[j][0] < face_depths[j + 1][0]:
                tmp = face_depths[j]
                face_depths[j] = face_depths[j + 1]
                face_depths[j + 1] = tmp

    # Draw each face as two triangles
    for entry in face_depths:
        avg_z = entry[0]
        indices = entry[1]
        color = entry[2]
        skip = False
        for i in indices:
            if projected[i] is None:
                skip = True
                break
        if skip:
            continue
        pts = []
        for i in indices:
            pts.append(projected[i])
        c = (color[0], color[1], color[2], 1.0)
        g.quad(
            g.vertex(pts[0][0], pts[0][1] + 35.0, z=max(0.0, min(1.0, pts[0][2] * 0.15)), color=c),
            g.vertex(pts[1][0], pts[1][1] + 35.0, z=max(0.0, min(1.0, pts[1][2] * 0.15)), color=c),
            g.vertex(pts[2][0], pts[2][1] + 35.0, z=max(0.0, min(1.0, pts[2][2] * 0.15)), color=c),
            g.vertex(pts[3][0], pts[3][1] + 35.0, z=max(0.0, min(1.0, pts[3][2] * 0.15)), color=c),
        )

    # =====================================================================
    # PART 4: DEPTH BUFFER FUNCTION & DEPTH MASK
    # =====================================================================
    # depth_buffer_function() changes the comparison used for depth testing.
    # Default is CMP_LESS (closer pixels win). Here we use CMP_GREATER
    # so that ONLY pixels farther than existing depth are drawn.
    #
    # depth_mask(False) disables writing to the depth buffer. This is
    # useful for "see-through" overlays like wireframes or holograms
    # that you want to render on top without affecting future draws.
    #
    # We use push_state() / pop_state() to isolate these changes so we
    # don't have to manually remember and restore every setting.

    # Draw a soft wireframe outline of the cube.
    g.push_state()
    g.set_blend_alpha()
    for entry in face_depths:
        avg_z = entry[0]
        indices = entry[1]
        color = entry[2]
        skip = False
        for i in indices:
            if projected[i] is None:
                skip = True
                break
        if skip:
            continue
        pts = []
        for i in indices:
            pts.append(projected[i])
        # Draw edges as white lines
        for i in range(4):
            j = (i + 1) % 4
            g.line(
                g.vertex(pts[i][0], pts[i][1] + 35.0, z=max(0.0, min(1.0, pts[i][2] * 0.15)), color=(1.0, 1.0, 1.0, 0.28)),
                g.vertex(pts[j][0], pts[j][1] + 35.0, z=max(0.0, min(1.0, pts[j][2] * 0.15)), color=(1.0, 1.0, 1.0, 0.28)),
            )
    g.pop_state()

    # =====================================================================
    # PART 4: 2D OVERLAY on top of 3D scene
    # =====================================================================
    # Switch back to 2D mode to draw HUD elements on top.
    # begin_2d() disables depth testing so overlays always appear on top.

    g.begin_2d()
    g.set_blend_none()
    g.rect(5.0, 230.0, 310.0, 8.0, (0.15, 0.15, 0.15))
    g.rect(5.0, 230.0, t * 20.0 % 310.0, 8.0, (0.3, 0.6, 1.0))
