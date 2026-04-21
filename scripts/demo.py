import math

import glide as g


state = {
    "t": 0.0,
    "speed": 1.0,
    "tilt": 0.0,
    "pulse": 0.0,
    "grid_phase": 0.0,
    "title_timer": 0.0,
}


def clamp(x, lo, hi):
    if x < lo:
        return lo
    if x > hi:
        return hi
    return x


def mix(a, b, t):
    return a + (b - a) * t


def rgba255(r, g, b, a=255):
    return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)


def format_speed(value):
    scaled = int(value * 100.0 + 0.5)
    whole = scaled // 100
    frac = scaled % 100
    if frac < 10:
        return str(whole) + ".0" + str(frac)
    return str(whole) + "." + str(frac)


def poly(points, color):
    if len(points) < 3:
        return
    v0 = g.vertex(points[0][0], points[0][1], 0.0, 1.0, color)
    for i in range(1, len(points) - 1):
        v1 = g.vertex(points[i][0], points[i][1], 0.0, 1.0, color)
        v2 = g.vertex(points[i + 1][0], points[i + 1][1], 0.0, 1.0, color)
        g.triangle(v0, v1, v2)


def circle(cx, cy, radius, color, steps=24):
    points = []
    for i in range(steps):
        a = (i / steps) * math.pi * 2.0
        points.append((cx + math.cos(a) * radius, cy + math.sin(a) * radius))
    poly(points, color)


def glow(cx, cy, radius, color, layers=4):
    for i in range(layers, 0, -1):
        t = i / layers
        r = radius * (0.45 + t * 0.75)
        a = color[3] * (0.08 + t * 0.18)
        circle(cx, cy, r, (color[0], color[1], color[2], a), 28)


def diamond(cx, cy, radius, color, inner=None):
    poly([
        (cx, cy - radius),
        (cx + radius * 0.9, cy),
        (cx, cy + radius),
        (cx - radius * 0.9, cy),
    ], color)
    if inner is not None:
        poly([
            (cx, cy - radius * 0.45),
            (cx + radius * 0.42, cy),
            (cx, cy + radius * 0.45),
            (cx - radius * 0.42, cy),
        ], inner)


def chevron(cx, cy, w, h, color):
    poly([
        (cx - w, cy - h),
        (cx, cy + h),
        (cx + w, cy - h),
        (cx + w * 0.5, cy - h),
        (cx, cy + h * 0.3),
        (cx - w * 0.5, cy - h),
    ], color)


def star(x, y, s, color):
    g.rect(x, y, s, 1.0, color)
    g.rect(x, y, 1.0, s, color)


def mountain(x, base_y, width, height, color):
    g.triangle(
        g.vertex(x, base_y, color=color),
        g.vertex(x + width * 0.5, base_y - height, color=color),
        g.vertex(x + width, base_y, color=color),
    )


def sun(cx, cy, radius):
    g.set_blend_add()
    glow(cx, cy, radius * 1.3, rgba255(255, 80, 180, 220), 5)
    glow(cx, cy, radius * 0.78, rgba255(255, 210, 120, 110), 3)
    g.set_blend_none()

    circle(cx, cy, radius * 1.02, rgba255(70, 30, 90, 180), 30)

    for band in range(10):
        t = band / 9.0
        band_y = cy - radius + t * radius * 1.75
        inset = (band * band) * 0.38
        color = (
            mix(1.0, 1.0, t),
            mix(0.86, 0.52, t),
            mix(0.24, 0.65, t),
            1.0,
        )
        g.rect(cx - radius + inset, band_y, radius * 2.0 - inset * 2.0, radius * 0.18, color)

    g.set_blend_add()
    g.rect(cx - radius * 1.1, cy + radius * 0.25, radius * 2.2, 2.0, rgba255(255, 90, 210, 90))
    g.set_blend_none()


def skyline(t):
    for i in range(11):
        x = i * 30 - 8
        wobble = math.sin(t * 0.7 + i * 1.13) * 6.0
        w = 16 + ((i * 7) % 18)
        h = 26 + ((i * 19) % 42)
        c = rgba255(20 + i * 6, 16, 48 + i * 8)
        g.rect(x, 142 - h + wobble * 0.15, w, h, c)
        for j in range(3):
            wx = x + 3 + j * 5
            wy = 146 - h + 8
            for k in range(3):
                if ((i + j + k) % 2) == 0:
                    g.rect(wx, wy + k * 8, 2, 4, rgba255(255, 160, 70, 180))


def grid(t):
    horizon = 138.0 + math.sin(t * 0.4) * 2.0
    magenta = rgba255(255, 60, 220)
    cyan = rgba255(40, 235, 255)
    for i in range(14):
        depth = i / 13.0
        y = horizon + depth * depth * 102.0
        glow = mix(0.15, 0.8, 1.0 - depth)
        c = (cyan[0] * glow, cyan[1] * glow, cyan[2] * glow, 1.0)
        g.line(g.vertex(0.0, y, color=c), g.vertex(320.0, y, color=c))

    shift = state["tilt"] * 34.0 + math.sin(t * 0.6) * 8.0
    for i in range(-10, 11):
        x = 160.0 + i * 22.0
        c = magenta if i % 2 == 0 else cyan
        g.line(g.vertex(160.0 + shift * 0.15, horizon, color=c), g.vertex(x + shift, 240.0, color=c))


def orbiters(t):
    mx, my = g.mouse_position()
    target_x = clamp(mx, 40.0, 280.0)
    target_y = clamp(my, 40.0, 200.0)

    for i in range(3):
        a = t * (0.8 + i * 0.25) + i * 2.1
        x = target_x + math.cos(a) * (28.0 + i * 16.0)
        y = target_y + math.sin(a * 1.7) * (10.0 + i * 5.0)
        r = 7.0 + i * 2.0 + math.sin(t * 3.0 + i) * 1.5
        ship = rgba255(120 + i * 45, 240 - i * 35, 255, 225)
        core = rgba255(255, 255, 255, 255)
        trail = rgba255(255, 120 + i * 30, 220, 70)

        g.set_blend_add()
        glow(x, y, r * 1.9, ship, 4)
        chevron(x, y + r * 1.35, r * 0.55, r * 0.7, trail)
        g.set_blend_none()
        diamond(x, y, r, ship, core)
        circle(x, y, r * 0.28, core, 12)
        g.line(g.vertex(target_x, target_y, color=rgba255(255, 255, 255, 80)),
               g.vertex(x, y, color=rgba255(255, 255, 255, 20)))


def satellites(t):
    for i in range(2):
        a = t * (0.35 + i * 0.12) + i * 2.7
        x = 160.0 + math.cos(a) * (92.0 + i * 18.0)
        y = 64.0 + math.sin(a * 1.6) * (22.0 + i * 8.0)
        s = 5.0 + i * 1.5
        g.set_blend_add()
        glow(x, y, s * 2.3, rgba255(120, 210, 255, 90), 3)
        g.set_blend_none()
        diamond(x, y, s, rgba255(40, 70, 100, 255), rgba255(200, 245, 255, 255))
        g.line(g.vertex(x - s * 1.6, y, color=rgba255(255, 255, 255, 140)),
               g.vertex(x + s * 1.6, y, color=rgba255(255, 255, 255, 60)))


def draw_stars(t):
    for i in range(48):
        x = (i * 53.0 + math.sin(t * 0.13 + i * 1.91) * 90.0 + t * (6.0 + (i % 5))) % 320.0
        y = 18.0 + ((i * 37.0) % 96.0) + math.sin(t * 0.5 + i) * 2.0
        twinkle = 0.4 + 0.6 * (0.5 + 0.5 * math.sin(t * 3.0 + i * 2.7))
        color = (twinkle, twinkle, twinkle * 1.2, 1.0)
        star(x, y, 2.0 if (i % 3) == 0 else 1.0, color)


def foreground_shards(t):
    for i in range(5):
        base_x = 36.0 + i * 58.0 + math.sin(t * 0.8 + i) * 4.0
        base_y = 196.0 + (i % 2) * 8.0
        glow_amt = 0.5 + 0.5 * math.sin(t * 2.5 + i * 0.7)
        c = (0.3 + glow_amt * 0.5, 0.9, 1.0, 0.85)
        g.triangle(
            g.vertex(base_x, base_y, color=c),
            g.vertex(base_x + 12.0, base_y - 36.0, color=(0.1, 0.4, 0.8, 0.2)),
            g.vertex(base_x + 28.0, base_y, color=c),
        )


def controls(dt):
    if g.key_down("left"):
        state["tilt"] -= dt * 1.4
    if g.key_down("right"):
        state["tilt"] += dt * 1.4
    if g.key_down("up"):
        state["speed"] += dt * 0.8
    if g.key_down("down"):
        state["speed"] -= dt * 0.8
    if g.key_down("space"):
        state["pulse"] += dt * 2.5

    state["speed"] = clamp(state["speed"], 0.3, 3.0)
    state["tilt"] *= 1.0 - min(dt * 3.0, 0.2)


def load():
    g.set_title("glint | python runtime demo")


def update(dt):
    state["t"] += dt * state["speed"]
    state["grid_phase"] += dt * (20.0 + state["speed"] * 18.0)
    state["pulse"] *= 1.0 - min(dt * 2.5, 0.25)
    controls(dt)

    state["title_timer"] += dt
    if state["title_timer"] > 0.2:
        state["title_timer"] = 0.0
        g.set_title("glint | python runtime demo | speed " + format_speed(state["speed"]))


def draw():
    t = state["t"]
    horizon = 138.0 + math.sin(t * 0.4) * 2.0

    g.clear(rgba255(9, 6, 20))
    g.set_untextured()
    draw_stars(t)

    sun(160.0 + math.sin(t * 0.35) * 10.0, 74.0, 46.0 + math.sin(t * 1.2) * 2.0)
    satellites(t)

    mountain(-30.0, horizon + 12.0, 150.0, 66.0, rgba255(22, 16, 46))
    mountain(48.0, horizon + 12.0, 134.0, 58.0, rgba255(32, 18, 62))
    mountain(130.0, horizon + 14.0, 120.0, 72.0, rgba255(18, 10, 34))
    mountain(215.0, horizon + 12.0, 132.0, 60.0, rgba255(28, 14, 54))

    skyline(t)
    grid(t + state["grid_phase"] * 0.01)

    g.set_blend_add()
    g.rect(0.0, horizon - 1.0, 320.0, 3.0, rgba255(255, 70, 220, 120))
    g.rect(0.0, horizon + 2.0, 320.0, 1.0, rgba255(40, 235, 255, 60))
    g.set_blend_none()

    orbiters(t + state["pulse"] * 0.2)
    foreground_shards(t)
