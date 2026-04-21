# glint Specification

> glint is a fantasy machine for making 3D games with the feel of the late 90s.
> Think: PICO-8's constraint-driven creativity, but for the 3dfx Voodoo / Quake era.

---

## 1. The Machine

glint is a runtime, not a library. Users write games in Python, bundle them into a `.glint` cartridge, and launch them with the `glint` executable.

The fantasy hardware profile:
- **CPU:** Pentium-class, single-core. All transforms and lighting run here.
- **RAM:** 32 MB total budget (enforced where practical).
- **GPU:** Fixed-function rasterizer. No hardware T&L. No hardware lighting.
- **Resolution:** 320×240, 4:3, scaled to window/screen with integer scaling.
- **Audio:** Sound Blaster-style synthesis + tracker music (MOD/XM/IT).
- **Input:** Keyboard, mouse, gamepad via GLFW.
- **Storage:** `.glint` cartridge archive.

---

## 2. Rendering API

### 2.1 Philosophy

Exposes a **useful subset of Glide 2.2** via Python bindings. The API is designed to feel like programming a Voodoo 1, but with Pythonic ergonomics.

> The full Glide 2.2 Reference Manual (`glideref.htm`) is included in this repository as the canonical source for the underlying API semantics. The glint subset strips away vendor-specific hardware management, framebuffer access, anti-aliasing, and other low-level details that are irrelevant to a fantasy console, while preserving the core rasterization vocabulary: `grColorCombine`, `grAlphaBlendFunction`, `grDrawTriangle`, `grTexSource`, etc.

- **No hardware T&L.** You write vertex transforms in Python.
- **No hardware lighting.** You compute vertex colors in Python and pass them to the rasterizer.
- **Immediate-mode triangles.** The hardware only understands `draw_triangle(v1, v2, v3)`.

### 2.2 Vertex Format

Vertices are plain tuples/lists, not structs. Missing fields use sensible defaults:

```python
(x, y)                    # flat color (uses current color state)
(x, y, u, v)              # textured
(x, y, r, g, b)           # colored (0-255)
(x, y, r, g, b, u, v)     # colored + textured
(x, y, z, r, g, b, u, v)  # full
```

### 2.3 Drawing Functions

```python
draw_triangle(v1, v2, v3)
draw_line(v1, v2)       # optional, for debug UI
```

### 2.4 State Management

**Pre-canned modes** (cover 90% of use cases):

```python
set_mode("flat")              # constant color
set_mode("gouraud")           # vertex colors only
set_mode("textured")          # texture * constant color
set_mode("textured_gouraud")  # texture * vertex color (the Quake look)
set_mode("transparent")       # texture with alpha blending
```

**Raw Glide API** (for power users):

```python
grColorCombine(func, factor, local, other, invert)
grAlphaCombine(func, factor, local, other, invert)
grAlphaBlendFunction(rgb_sf, rgb_df, alpha_sf, alpha_df)
grConstantColorValue(color)
grDepthBufferMode(mode)
grDepthBufferFunction(func)
grDepthMask(enabled)
grCullMode(mode)
grClipWindow(xmin, ymin, xmax, ymax)
grShadeModel(mode)
```

### 2.5 Textures

```python
load_texture(name)          # loads from cartridge, returns handle
bind_texture(handle)
```

**Constraints:**
- Max texture size: **256×256**.
- Max simultaneously loaded textures: **8**.
- No runtime PNG/JPEG loading. Textures are stored in a runtime-ready format inside the cartridge.

### 2.6 Frame Control

```python
buffer_clear(color, alpha, depth)
buffer_swap()
```

### 2.7 2D / 3D Mode

```python
begin_3d()   # perspective projection, depth test enabled
begin_2d()   # orthographic projection, depth test disabled
```

---

## 3. Scripting Environment

### 3.1 Language

**pocketpy** (embedded C11 Python 3.x interpreter). No external Python installation required.

### 3.2 Game Loop

love2d-style callbacks:

```python
def load():
    # called once at startup
    pass

def update(dt):
    # called every frame
    pass

def draw():
    # called every frame after update
    pass

def keydown(key):
    pass

def keyup(key):
    pass
```

### 3.3 Standard Library

pocketpy provides a subset of Python's stdlib. Game code should avoid heavy allocations per frame — GC pressure is the user's problem, and that's part of the fun.

---

## 4. Input

Wrapped GLFW input, exposed as simple Python functions:

```python
key_down(key)      # bool
key_pressed(key)   # bool, true for one frame
mouse_x()          # int
mouse_y()          # int
mouse_down(button) # bool
```

No DirectInput complexity. Just state polling.

---

## 5. Audio (TBD)

### 5.1 Philosophy

Sound Blaster synthesis era. Not streaming MP3s. Small, crunchy, generated.

### 5.2 Planned API

```python
# Synthesis
synth_tone(freq, duration, waveform="square")
synth_noise(duration)

# Tracker music
play_music("track.xm")
stop_music()
```

### 5.3 Constraints

- Audio data is stored in the cartridge, not loaded from external files.
- Tracker formats (MOD, S3M, XM, IT) are the preferred music format.

---

## 6. Cartridge Format

### 6.1 File Extension

`.glint`

### 6.2 Format

Binary archive (not a zip). Structure:

```
[Header]
  magic: "GLINT\0"
  version: uint16
  code_offset: uint32
  code_size: uint32
  textures_offset: uint32
  textures_count: uint32
  audio_offset: uint32
  audio_count: uint32

[Code Section]
  UTF-8 Python source (or minified)

[Textures Section]
  Array of runtime-ready texture blobs (palette or 16-bit RGB + mipmaps)

[Audio Section]
  Array of audio blobs (tracker data or synth patches)
```

### 6.3 Distribution

The runtime loads a `.glint` file and executes it. No external dependencies, no loose files. One file per game.

---

## 7. Console

In-game tilde (`~`) console, Quake-style. Exposed to the Python game for logging and debug commands.

```python
console_print("spawned enemy at " + str(x))
```

The runtime itself uses the console for errors and warnings.

---

## 8. Memory & Performance Constraints

| Limit | Value | Rationale |
|-------|-------|-----------|
| Resolution | 320×240 | Voodoo 1 era base resolution |
| Max textures | 8 | ~2MB texture RAM feel |
| Max texture size | 256×256 | Voodoo 1 constraints |
| Target RAM | 32 MB | Pentium + 32MB era |
| Frame rate | Uncapped / VSync | GLFW default |

---

## 9. Build & Dependencies

### 9.1 Runtime Stack

| Component | Technology |
|-----------|------------|
| Language | C11 |
| Build system | ninja (generated by `tools/gen_build_ninja.py`) |
| Windowing / Input | GLFW |
| OpenGL loader | GLAD (OpenGL ES 2.0) |
| Scripting | pocketpy |
| Audio | TBD (miniaudio or custom) |

### 9.2 What's NOT Included

Consistent with the era, the runtime avoids modern convenience libraries:

- ❌ No zlib in runtime (custom compression or none)
- ❌ No libpng / libjpeg in runtime (custom texture format)
- ❌ No physics engine
- ❌ No networking
- ❌ No asset pipeline in the runtime (pre-processed at build time)

### 9.3 Build Targets

```bash
ninja          # release build
ninja debug    # debug build
ninja clean    # remove build artifacts
```

---

## 10. Open Questions

1. **Audio implementation:** miniaudio vs custom synth engine?
2. **Cartridge compression:** Custom RLE/Huffman or adopt zlib for size?
3. **Matrix math:** Expose a `glint.math` module with basic 3D math, or force users to write their own?
4. **Font rendering:** Bitmap font API, or users draw text with triangles?
5. **Asset pipeline:** How do users convert PNGs/textures into `.glint` format? Separate tool?

---

## Appendix A: Reference Material

- **Glide 2.2 Reference Manual** (`glideref.htm`) — Complete API documentation for the original 3dfx Voodoo Graphics library. This is the source material from which the glint rendering subset is derived.

---

*glint: a tiny retro-future fantasy PC for making evocative 3D worlds with the fewest moving parts possible.*
