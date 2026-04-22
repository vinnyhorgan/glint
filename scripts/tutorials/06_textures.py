# =============================================================================
# Tutorial 06 — Textures
# =============================================================================
#
# What you'll learn:
#   - Procedurally generating pixel data in Python
#   - upload_texture() to create a GPU texture
#   - image() to draw textured quads
#   - set_mode('textured'), 'textured_gouraud', 'transparent'
#   - Texture formats: ARGB8888 (the easiest to use)
#   - Texture filtering: point-sampled vs bilinear
#   - Texture clamping: wrap vs clamp-to-edge
#   - Sub-region UVs for sprite-sheet-style usage
#
# A texture is a 2D image that can be mapped onto geometry. In glint,
# textures are uploaded as raw pixel arrays from Python.
#
# The texture pipeline:
#   1. Generate or load pixel data (a flat list of byte values)
#   2. Call upload_texture() to send it to the GPU
#   3. Set a textured render mode (set_mode or set_textured_modulate)
#   4. Draw geometry with UV coordinates (0.0-1.0 range)
#
# Maximum 8 textures, each up to 256x256 pixels.
#
# =============================================================================

import math
import glide as g


state = {"t": 0.0, "tex0": -1, "tex1": -1, "tex2": -1, "tex3": -1}


# =============================================================================
# PROCEDURAL TEXTURE GENERATORS
# =============================================================================
# Textures are uploaded as flat byte arrays in ARGB8888 format.
# That means each pixel is 4 bytes: A, R, G, B (in that order).
# A = alpha, R = red, G = green, B = blue, each 0-255.

def make_checkerboard(size=32, cell=4):
    """Classic checkerboard pattern — good for testing UV mapping."""
    pixels = []
    for y in range(size):
        for x in range(size):
            is_white = ((x // cell) + (y // cell)) % 2 == 0
            if is_white:
                pixels.extend([255, 240, 240, 240])  # A, R, G, B
            else:
                pixels.extend([255, 60, 60, 80])
    return pixels


def make_gradient(size=32):
    """A diagonal color gradient — shows off bilinear filtering."""
    pixels = []
    for y in range(size):
        for x in range(size):
            r = int((x / (size - 1)) * 255)
            g = int((y / (size - 1)) * 255)
            b = int(((x + y) / ((size - 1) * 2)) * 255)
            pixels.extend([255, r, g, b])
    return pixels


def make_sprite_sheet(size=64):
    """A 4x4 grid of colored squares — demonstrates UV sub-regions."""
    cell = size // 4
    pixels = []
    palette = [
        (255, 80, 80), (80, 255, 80), (80, 80, 255), (255, 255, 80),
        (255, 80, 255), (80, 255, 255), (255, 160, 80), (160, 80, 255),
        (80, 255, 160), (255, 120, 180), (180, 255, 120), (120, 180, 255),
        (200, 200, 200), (255, 200, 150), (150, 255, 200), (200, 150, 255),
    ]
    for y in range(size):
        for x in range(size):
            cx = x // cell
            cy = y // cell
            idx = cy * 4 + cx
            r, gr, b = palette[idx]
            pixels.extend([255, r, gr, b])
    return pixels


# =============================================================================
# LOAD — create textures once at startup
# =============================================================================

def load():
    g.set_title("06 — Textures")

    # upload_texture(width, height, pixels, ...) uploads pixel data to the GPU.
    # Default format is ARGB_8888 (4 bytes per pixel: A, R, G, B).
    # Returns a texture ID (integer) that you use to reference it.
    #
    # Optional parameters:
    #   fmt          — TEXFMT_ARGB_8888 (default), TEXFMT_RGB_565, TEXFMT_ARGB_1555
    #   min_filter   — TEXTUREFILTER_POINT_SAMPLED (default) or TEXTUREFILTER_BILINEAR
    #   mag_filter   — defaults to same as min_filter
    #   mipmap       — MIPMAP_DISABLE (default), MIPMAP_NEAREST
    #   s_clamp      — TEXTURECLAMP_WRAP (default) or TEXTURECLAMP_CLAMP
    #   t_clamp      — defaults to same as s_clamp
    #
    # Under the hood, upload_texture() calls the lower-level Glide API:
    #   tex = tex_allocate()        # grab a free texture slot (max 8)
    #   tex_download_mipmap(tex, ...) # upload pixel data
    #   tex_filter(tex, ...)        # set filtering
    #   tex_clamp_mode(tex, ...)    # set wrapping
    # When you're done with a texture, call tex_free(tex) to release it.

    state["tex0"] = g.upload_texture(32, 32, make_checkerboard(32, 4))

    # This texture uses bilinear filtering for smooth interpolation
    state["tex1"] = g.upload_texture(
        32, 32, make_gradient(32),
        min_filter=g.TEXTUREFILTER_BILINEAR,
    )

    # Sprite sheet — 64x64 with 4x4 cells
    state["tex2"] = g.upload_texture(64, 64, make_sprite_sheet(64))

    # Clamp texture — created once in load(), not every frame
    state["tex3"] = g.upload_texture(
        32, 32, make_checkerboard(32, 4),
        s_clamp=g.TEXTURECLAMP_CLAMP,
        t_clamp=g.TEXTURECLAMP_CLAMP,
    )


def update(dt):
    state["t"] += dt


def draw():
    t = state["t"]
    g.clear((0.02, 0.02, 0.06, 1.0))

    # =====================================================================
    # BASIC TEXTURED QUAD
    # =====================================================================
    # set_mode('textured') sets up the combine unit to show the texture
    # with a constant tint color (white = no tint).
    # image(tex, x, y, w, h) draws a textured quad.

    g.set_mode("textured")
    g.image(state["tex0"], 10.0, 10.0, 80.0, 80.0)

    # =====================================================================
    # BILINEAR FILTERED TEXTURE
    # =====================================================================
    # Compare: the checkerboard above uses point sampling (crisp pixels),
    # while this gradient uses bilinear filtering (smooth blending).

    g.image(state["tex1"], 100.0, 10.0, 80.0, 80.0)

    # =====================================================================
    # TINTED TEXTURE (textured_gouraud mode)
    # =====================================================================
    # set_mode('textured_gouraud') multiplies the texture color by the
    # vertex color. This lets you tint a texture per-vertex.
    #
    # With white vertices, the texture appears unchanged.
    # With colored vertices, the texture gets tinted.

    g.set_mode("textured_gouraud")

    # Red-tinted checkerboard
    g.image(state["tex0"], 200.0, 10.0, 80.0, 80.0,
            color=(1.0, 0.3, 0.3, 1.0))

    # =====================================================================
    # ANIMATED TINT
    # =====================================================================
    # The color parameter of image() can animate over time.

    r = 0.5 + 0.5 * math.sin(t * 1.5)
    gr = 0.5 + 0.5 * math.sin(t * 1.5 + 2.1)
    b = 0.5 + 0.5 * math.sin(t * 1.5 + 4.2)
    g.image(state["tex0"], 10.0, 100.0, 60.0, 60.0,
            color=(r, gr, b, 1.0))

    # =====================================================================
    # SPRITE SHEET — UV SUB-REGIONS
    # =====================================================================
    # image() supports u0, v0, u1, v1 parameters to select a sub-region
    # of the texture. UVs are in 0.0-1.0 range.
    #
    # Our sprite sheet is 4x4, so each cell is 0.25 x 0.25 in UV space.

    # Draw a 2x2 selection of cells from the sprite sheet
    for row in range(2):
        for col in range(2):
            u0 = col * 0.25
            v0 = row * 0.25
            u1 = u0 + 0.25
            v1 = v0 + 0.25
            x = 80.0 + col * 36.0
            y = 100.0 + row * 36.0
            g.image(state["tex2"], x, y, 32.0, 32.0,
                    u0=u0, v0=v0, u1=u1, v1=v1)

    # Animated sprite selection — cycles through the 4x4 grid
    frame = int(t * 4.0) % 16
    fx = frame % 4
    fy = frame // 4
    g.image(state["tex2"], 170.0, 100.0, 48.0, 48.0,
            u0=fx * 0.25, v0=fy * 0.25,
            u1=(fx + 1) * 0.25, v1=(fy + 1) * 0.25)

    # =====================================================================
    # TRANSPARENT TEXTURE
    # =====================================================================
    # set_mode('transparent') enables texture + alpha blending.
    # This is needed when your texture has alpha transparency.
    #
    # We'll create a simple "cutout" effect: draw the gradient texture
    # with per-vertex alpha for a fade-out effect.

    g.set_mode("transparent")

    # The image() function uses a constant color. For per-vertex alpha
    # we need to draw manually with vertex() and triangle():
    g.tex_bind(state["tex1"])

    # Draw a quad with fading alpha from left to right
    x0, y0 = 10.0, 175.0
    x1, y1 = 150.0, 230.0
    g.quad(
        g.vertex(x0, y0, color=(1.0, 1.0, 1.0, 1.0), u=0.0, v=0.0),
        g.vertex(x1, y0, color=(1.0, 1.0, 1.0, 1.0), u=1.0, v=0.0),
        g.vertex(x1, y1, color=(1.0, 1.0, 1.0, 0.2), u=1.0, v=1.0),
        g.vertex(x0, y1, color=(1.0, 1.0, 1.0, 0.2), u=0.0, v=1.0),
    )

    # =====================================================================
    # TEXTURE CLAMPING — wrap vs clamp-to-edge
    # =====================================================================
    # By default textures WRAP (repeat) when UVs go outside 0-1.
    # With CLAMP, the edge pixel is stretched instead.
    #
    # Here we draw the same checkerboard with UVs from -0.5 to 1.5
    # (showing 2x2 repeats). The top uses WRAP (default), the bottom
    # uses CLAMP (no repetition, just edge stretching).

    g.set_mode("textured")

    # WRAP texture (default) — the checkerboard tiles repeatedly
    g.tex_bind(state["tex0"])
    g.quad(
        g.vertex(165.0, 165.0, u=-0.5, v=-0.5),
        g.vertex(235.0, 165.0, u=1.5,  v=-0.5),
        g.vertex(235.0, 200.0, u=1.5,  v=1.5),
        g.vertex(165.0, 200.0, u=-0.5, v=1.5),
    )

    # CLAMP texture — edge pixels stretch, no tiling
    g.tex_bind(state["tex3"])
    g.quad(
        g.vertex(245.0, 165.0, u=-0.5, v=-0.5),
        g.vertex(315.0, 165.0, u=1.5,  v=-0.5),
        g.vertex(315.0, 200.0, u=1.5,  v=1.5),
        g.vertex(245.0, 200.0, u=-0.5, v=1.5),
    )

    # =====================================================================
    # SCALED AND ROTATED TEXTURE (manual vertex placement)
    # =====================================================================
    # By placing vertices at arbitrary positions, we can skew, scale, and
    # create perspective effects with textures.

    g.set_mode("textured")
    cx, cy = 240.0, 200.0
    s = 25.0 + math.sin(t * 1.2) * 8.0
    angle = t * 0.5
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)

    # Four corners of a rotating quad
    corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    verts = []
    for ux, uy in corners:
        rx = ux * s * cos_a - uy * s * sin_a
        ry = ux * s * sin_a + uy * s * cos_a
        verts.append(g.vertex(cx + rx, cy + ry,
                              color=(1.0, 1.0, 1.0, 1.0),
                              u=(ux + 1) * 0.5, v=(uy + 1) * 0.5))

    g.tex_bind(state["tex0"])
    g.quad(verts[0], verts[1], verts[2], verts[3])
