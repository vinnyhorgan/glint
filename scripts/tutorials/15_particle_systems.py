# =============================================================================
# Tutorial 15 — Particle Systems
# =============================================================================
#
# What you'll learn:
#   - A simple particle system with spawn, update, and render
#   - Particle properties: position, velocity, lifetime, color, size
#   - Continuous emission vs burst emission
#   - Additive blending for fire, sparks, and glow effects
#   - Alpha fade-out over lifetime
#   - Simple physics: gravity, drag, and bounce
#
# Particles are tiny sprites that are spawned, animated, and destroyed
# every frame. They're perfect for fire, smoke, sparks, explosions,
# magic effects, rain, snow, and more.
#
# The basic loop:
#   1. Spawn new particles (continuous or burst)
#   2. Update existing particles (move, age, apply forces)
#   3. Remove dead particles (lifetime expired)
#   4. Draw all surviving particles
#
# =============================================================================

import math
import glide as g


# =============================================================================
# SIMPLE PSEUDO-RANDOM GENERATOR
# =============================================================================
# pocketpy doesn't include the 'random' module, so we roll our own.
# This is a Linear Congruential Generator (LCG) — fast and good enough
# for visual effects. The seed changes every frame so patterns don't repeat.

_rand_state = 12345

def _rand_seed(t):
    global _rand_state
    _rand_state = int(t * 1000000) & 0x7FFFFFFF

def _rand():
    """Return a random float in [0.0, 1.0)."""
    global _rand_state
    _rand_state = (_rand_state * 1103515245 + 12345) & 0x7FFFFFFF
    return _rand_state / 2147483648.0

def _rand_range(lo, hi):
    """Return a random float in [lo, hi)."""
    return lo + _rand() * (hi - lo)

def _rand_choice(items):
    """Return a random item from a list."""
    return items[int(_rand() * len(items))]


# =============================================================================
# PARTICLE SYSTEM STATE
# =============================================================================

state = {
    "t": 0.0,
    "particles": [],       # list of active particles
    "emitters": [],        # list of emitter configs
    "tex_spark": -1,
    "tex_smoke": -1,
}


# =============================================================================
# PROCEDURAL PARTICLE TEXTURES
# =============================================================================
# Each particle is drawn as a small textured quad. Procedural textures
# give us soft circles (for sparks) and puffy clouds (for smoke).

def make_spark_texture(size=16):
    """A soft glowing circle — perfect for sparks and fire."""
    pixels = []
    cx = size // 2
    cy = size // 2
    for y in range(size):
        for x in range(size):
            dx = x - cx
            dy = y - cy
            d = math.sqrt(dx * dx + dy * dy) / (size // 2)
            if d > 1.0:
                alpha = 0
            else:
                # Smooth falloff from center
                alpha = int(255 * (1.0 - d * d))
            pixels.extend([alpha, 255, 220, 180])
    return pixels


def make_smoke_texture(size=16):
    """A soft puffy cloud — good for smoke and steam."""
    pixels = []
    cx = size // 2
    cy = size // 2
    for y in range(size):
        for x in range(size):
            dx = x - cx
            dy = y - cy
            # Distorted distance for puffier look
            d = math.sqrt(dx * dx + dy * dy * 0.7) / (size // 2)
            if d > 1.0:
                alpha = 0
            else:
                alpha = int(200 * (1.0 - d) * (1.0 - d))
            pixels.extend([alpha, 220, 220, 220])
    return pixels


# =============================================================================
# PARTICLE HELPERS
# =============================================================================

def spawn_particle(x, y, vx, vy, lifetime, color, size, texture,
                   gravity=0.0, drag=1.0, growth=0.0):
    """Create a new particle and add it to the system."""
    p = {
        "x": x,
        "y": y,
        "vx": vx,
        "vy": vy,
        "life": lifetime,
        "max_life": lifetime,
        "color": color,
        "size": size,
        "texture": texture,
        "gravity": gravity,
        "drag": drag,
        "growth": growth,
    }
    state["particles"].append(p)


def spawn_burst(cx, cy, count, speed, lifetime, color, size, texture,
                gravity=0.0, drag=1.0):
    """Spawn a burst of particles radiating outward from a point."""
    for _ in range(count):
        angle = _rand() * math.pi * 2.0
        spd = speed * (0.5 + _rand() * 0.5)
        vx = math.cos(angle) * spd
        vy = math.sin(angle) * spd
        spawn_particle(cx, cy, vx, vy, lifetime, color, size, texture,
                       gravity=gravity, drag=drag)


def update_particles(dt):
    """Update all particles: apply physics, age, remove dead ones."""
    alive = []
    for p in state["particles"]:
        # Apply gravity
        p["vy"] += p["gravity"] * dt
        # Apply drag (velocity decay)
        p["vx"] *= p["drag"]
        p["vy"] *= p["drag"]
        # Move
        p["x"] += p["vx"] * dt
        p["y"] += p["vy"] * dt
        # Grow/shrink
        p["size"] += p["growth"] * dt
        # Age
        p["life"] -= dt
        if p["life"] > 0.0 and p["size"] > 0.0:
            alive.append(p)
    state["particles"] = alive


def draw_particles():
    """Draw all particles as textured quads with lifetime-based alpha."""
    g.set_mode("transparent")
    g.set_blend_add()

    for p in state["particles"]:
        # Alpha fades from full to zero over the particle's lifetime
        t = 1.0 - (p["life"] / p["max_life"])
        alpha = 1.0 - t * t  # quadratic fade (brighter at start)
        if alpha < 0.0:
            alpha = 0.0

        c = p["color"]
        color = (c[0], c[1], c[2], c[3] * alpha)
        s = p["size"]

        g.image(p["texture"],
                p["x"] - s * 0.5, p["y"] - s * 0.5,
                s, s,
                color=color)


# =============================================================================
# LOAD
# =============================================================================

def load():
    g.set_title("15 — Particle Systems (click = spark burst, space = rocket)")
    state["tex_spark"] = g.upload_texture(16, 16, make_spark_texture(16))
    state["tex_smoke"] = g.upload_texture(16, 16, make_smoke_texture(16))


# =============================================================================
# UPDATE
# =============================================================================

def update(dt):
    t = state["t"] + dt
    state["t"] = t

    # Seed the RNG so particle patterns vary each frame
    _rand_seed(t)

    # =====================================================================
    # CONTINUOUS EMISSION: campfire
    # =====================================================================
    # A steady stream of fire particles rising from the center-bottom.
    if _rand() < 0.4:
        x = 80.0 + _rand_range(-4.0, 4.0)
        y = 210.0
        vx = _rand_range(-8.0, 8.0)
        vy = _rand_range(-60.0, -40.0)
        life = _rand_range(0.4, 0.8)
        # Color shifts from yellow-orange to red
        r = 1.0
        gr = _rand_range(0.4, 0.8)
        b = _rand_range(0.0, 0.2)
        size = _rand_range(4.0, 8.0)
        spawn_particle(x, y, vx, vy, life, (r, gr, b, 1.0), size,
                       state["tex_spark"], gravity=-20.0, drag=0.97, growth=-2.0)

    # =====================================================================
    # CONTINUOUS EMISSION: smoke
    # =====================================================================
    # Smoke rises slower and drifts sideways.
    if _rand() < 0.15:
        x = 80.0 + _rand_range(-2.0, 2.0)
        y = 190.0
        vx = _rand_range(-5.0, 5.0)
        vy = _rand_range(-20.0, -10.0)
        life = _rand_range(1.0, 2.0)
        size = _rand_range(6.0, 12.0)
        spawn_particle(x, y, vx, vy, life, (0.7, 0.7, 0.7, 0.4), size,
                       state["tex_smoke"], gravity=-8.0, drag=0.98, growth=3.0)

    # =====================================================================
    # BURST EMISSION: mouse click sparks
    # =====================================================================
    # When the left mouse button is held, emit a shower of sparks.
    if g.mouse_down("left"):
        mx = float(g.mouse_x())
        my = float(g.mouse_y())
        # Emit a few sparks per frame
        for _ in range(2):
            angle = _rand() * math.pi * 2.0
            spd = _rand_range(30.0, 80.0)
            vx = math.cos(angle) * spd
            vy = math.sin(angle) * spd
            life = _rand_range(0.3, 0.6)
            color = _rand_choice([
                (1.0, 0.9, 0.3, 1.0),   # gold
                (1.0, 0.5, 0.2, 1.0),   # orange
                (0.9, 0.9, 1.0, 1.0),   # white-blue
            ])
            spawn_particle(mx, my, vx, vy, life, color, _rand_range(3.0, 6.0),
                           state["tex_spark"], gravity=60.0, drag=0.95)

    # =====================================================================
    # BURST EMISSION: rocket launch (space key)
    # =====================================================================
    # A single press launches a stream of fast particles upward.
    if g.key_pressed("space"):
        spawn_burst(240.0, 210.0, 30, 120.0, 0.8,
                    (0.4, 0.7, 1.0, 1.0), 5.0, state["tex_spark"],
                    gravity=80.0, drag=0.92)

    # =====================================================================
    # UPDATE ALL PARTICLES
    # =====================================================================
    update_particles(dt)


# =============================================================================
# DRAW
# =============================================================================

def draw():
    g.clear((0.02, 0.02, 0.05, 1.0))

    # =====================================================================
    # BACKGROUND SCENE
    # =====================================================================
    # Draw a simple ground and some context so the particles feel anchored.

    g.set_untextured()
    g.set_blend_none()

    # Ground
    g.rect(0.0, 210.0, float(g.FB_W), 30.0, (0.08, 0.07, 0.06))

    # Campfire logs (simple brown rects)
    g.rect(70.0, 206.0, 20.0, 6.0, (0.3, 0.2, 0.1))
    g.rect(82.0, 202.0, 18.0, 6.0, (0.25, 0.18, 0.08))

    # Rocket pad
    g.rect(220.0, 200.0, 40.0, 12.0, (0.15, 0.15, 0.18))
    g.rect(235.0, 190.0, 10.0, 10.0, (0.5, 0.5, 0.6))

    # =====================================================================
    # DRAW PARTICLES
    # =====================================================================
    # Particles are drawn with additive blending so overlapping particles
    # get brighter, creating a natural glow.

    draw_particles()

    # =====================================================================
    # RESET BLENDING
    # =====================================================================
    g.set_blend_none()

    # =====================================================================
    # HUD
    # =====================================================================
    # Show how many particles are alive.

    count = len(state["particles"])
    bar_w = min(100.0, count * 0.5)
    g.rect(4.0, 4.0, 100.0, 6.0, (0.1, 0.1, 0.15))
    g.rect(4.0, 4.0, bar_w, 6.0, (0.3, 0.6, 1.0))

    # Instruction hint
    g.rect(4.0, 14.0, 60.0, 4.0, (0.15, 0.15, 0.2))
    g.rect(4.0, 22.0, 50.0, 4.0, (0.15, 0.15, 0.2))
