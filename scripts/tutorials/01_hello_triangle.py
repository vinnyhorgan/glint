# =============================================================================
# Tutorial 01 — Hello Triangle
# =============================================================================
#
# What you'll learn:
#   - The basic program structure: load(), update(dt), draw()
#   - How to import the glide module
#   - Screen coordinates (320 x 240 framebuffer)
#   - Clearing the screen
#   - Creating vertices and drawing a triangle
#   - The frame loop: update -> draw -> swap
#
# Every glint script must define three functions:
#
#   load()           — called once at startup (like Love2D's love.load)
#   update(dt)       — called every frame with dt in seconds
#   draw()           — called every frame after update()
#
# The runtime handles the main loop for you. All you do is fill in these
# callbacks. The framebuffer is 320 pixels wide and 240 pixels tall, giving
# a chunky retro pixel look when scaled up to the window.
#
# =============================================================================

import glide as g


# -- load() -------------------------------------------------------------------
# Called once when the script starts. Use it for one-time setup.
# Here we just set the window title.
# -----------------------------------------------------------------------------
def load():
    g.set_title("01 — Hello Triangle")


# -- update(dt) ---------------------------------------------------------------
# Called every frame before draw(). 'dt' is the time elapsed since the last
# frame in seconds (capped at 0.1s). Use it for physics, animation, input.
# We don't need any logic for this simple example.
# -----------------------------------------------------------------------------
def update(dt):
    pass


# -- draw() -------------------------------------------------------------------
# Called every frame after update(). This is where all rendering happens.
#
# The drawing pipeline is:
#   1. Clear the framebuffer
#   2. Set up render state (usually with a high-level mode helper)
#   3. Issue draw calls (triangles, lines, points)
#   4. The runtime calls swap() for you after draw() returns
#      (you do NOT call g.swap() yourself — the main loop does it)
# -----------------------------------------------------------------------------
def draw():

    # -- Clearing the screen --------------------------------------------------
    # clear() takes an optional RGBA color tuple. Values are 0.0-1.0.
    # This fills the entire 320x240 framebuffer with the given color and
    # resets the depth buffer.
    #
    # clear() is a convenience wrapper around buffer_clear(). The raw API
    # also provides buffer_clear(color, alpha, depth) for precise control.
    g.clear((0.05, 0.05, 0.12, 1.0))

    # -- Setting the render mode ----------------------------------------------
    # set_mode("gouraud") is the standard "vertex colors, no texture" mode.
    # Under the hood this selects the right combine state for untextured
    # drawing. The lower-level combine functions are covered later.
    g.set_mode("gouraud")

    # -- Creating vertices ----------------------------------------------------
    # A vertex is a point in screen space with optional color, depth, and
    # texture coordinates. The simplest form is a 2-tuple: (x, y).
    #
    # Screen coordinates:
    #   (0, 0) is the top-left corner
    #   (320, 240) is the bottom-right corner
    #
    # We'll make a white triangle centered on the screen.
    # The 5-tuple form is (x, y, r, g, b) — position plus color.
    # Color values can be either 0.0-1.0 floats or 0-255 convenience values.
    #
    #        (160, 40)
    #           /\
    #          /  \
    #         /    \
    #        /______\
    #   (60,200)  (260,200)
    #
    v0 = (160.0, 40.0,  1.0, 1.0, 1.0)  # top — white
    v1 = (60.0,  200.0, 1.0, 1.0, 1.0)  # bottom-left — white
    v2 = (260.0, 200.0, 1.0, 1.0, 1.0)  # bottom-right — white

    # -- Drawing a triangle ---------------------------------------------------
    # triangle() takes three vertices and rasterizes a filled triangle.
    # Vertices can be tuples or Vertex objects (covered later).
    g.triangle(v0, v1, v2)
