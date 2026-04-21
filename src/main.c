#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "miniaudio.h"
#include "pocketpy.h"

#define FB_W 320.0f
#define FB_H 240.0f
#define DEG2RAD(x) ((x) * 0.017453292519943295f)
#define VIEW_NEAR_Z 0.035f
#define VIEW_FAR_Z 40.0f

typedef struct {
    float x;
    float y;
    float z;
} V3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
} ClipVert;

typedef struct {
    V3 pos;
    float yaw;
    float pitch;
    int freelook;
    double last_mouse_x;
    double last_mouse_y;
    int mouse_seeded;
} Camera;

typedef struct {
    int tex_checker;
    int tex_gradient;
    int tex_ring;
    int tex_rgb565;
    int filter_mode;
    int fog_on;
    int blend_on;
    int alpha_test_on;
    int cull_on;
    int frames;
    double last_title;
    double last_frame;
    Camera cam;
} Demo;

static const V3 kCubeVerts[8] = {
    {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
    { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
    {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
    { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
};

static const int kCubeFaces[6][4] = {
    {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
    {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7},
};

static const float kQuadUV[4][2] = {
    {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
};

static void error_cb(int code, const char *desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

static GLADapiproc glad_loader(const char *name)
{
    return glfwGetProcAddress(name);
}

static void test_pocketpy(void)
{
    py_initialize();
    {
        py_GlobalRef mod = py_getmodule("__main__");
        if (!py_exec("print('pocketpy OK')", "<test>", EXEC_MODE, mod))
            py_printexc();
    }
    py_finalize();
}

static void test_miniaudio(void)
{
    ma_engine engine;
    if (ma_engine_init(NULL, &engine) == MA_SUCCESS) {
        fprintf(stderr, "miniaudio OK\n");
        ma_engine_uninit(&engine);
    } else {
        fprintf(stderr, "miniaudio init failed\n");
    }
}

static uint8_t *make_checker_rgba(int w, int h)
{
    uint8_t *pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            int tile = ((x >> 3) + (y >> 3)) & 1;
            pixels[i + 0] = tile ? 250 : 35;
            pixels[i + 1] = tile ? 205 : 60;
            pixels[i + 2] = tile ? 90 : 140;
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

static uint8_t *make_gradient_rgba(int w, int h)
{
    uint8_t *pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            float u = (float)x / (float)(w - 1);
            float v = (float)y / (float)(h - 1);
            pixels[i + 0] = (uint8_t)(40 + 180.0f * u);
            pixels[i + 1] = (uint8_t)(110 + 90.0f * v);
            pixels[i + 2] = (uint8_t)(220 - 130.0f * u);
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

static uint8_t *make_ring_rgba(int w, int h)
{
    uint8_t *pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    float cx = 0.5f * (float)w;
    float cy = 0.5f * (float)h;
    float inner = 0.24f * (float)w;
    float outer = 0.46f * (float)w;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float d = sqrtf(dx * dx + dy * dy);
            int inside = d >= inner && d <= outer;
            pixels[i + 0] = 255;
            pixels[i + 1] = 240;
            pixels[i + 2] = 180;
            pixels[i + 3] = inside ? 255 : 0;
        }
    }
    return pixels;
}

static uint16_t *make_rgb565(int w, int h)
{
    uint16_t *pixels = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int r = (x * 31) / (w - 1);
            int g = (y * 63) / (h - 1);
            int b = 31 - (x * 31) / (w - 1);
            pixels[y * w + x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
    return pixels;
}

static void apply_filter_mode(const Demo *demo)
{
    GrTextureFilter filter = demo->filter_mode == 0
        ? GR_TEXTUREFILTER_POINT_SAMPLED
        : GR_TEXTUREFILTER_BILINEAR;
    GrMipMapMode mip = demo->filter_mode == 2 ? GR_MIPMAP_NEAREST : GR_MIPMAP_DISABLE;

    grTexFilter(demo->tex_checker, mip, filter, filter);
    grTexFilter(demo->tex_gradient, mip, filter, filter);
    grTexFilter(demo->tex_ring, GR_MIPMAP_DISABLE,
                GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexFilter(demo->tex_rgb565, mip, filter, filter);
}

static GrVertex v2d(float x, float y, float z, GrColor_t color, float alpha, float u, float v)
{
    GrVertex out;
    float r, g, b, a;

    grColorUnpack(color, &r, &g, &b, &a);
    out.x = (x / (FB_W * 0.5f)) - 1.0f;
    out.y = (y / (FB_H * 0.5f)) - 1.0f;
    out.z = z;
    out.oow = 1.0f;
    out.r = r;
    out.g = g;
    out.b = b;
    out.a = alpha;
    out.u = u;
    out.v = v;
    return out;
}

static V3 rotate_x(V3 p, float a)
{
    float c = cosf(a);
    float s = sinf(a);
    V3 out = {p.x, p.y * c - p.z * s, p.y * s + p.z * c};
    return out;
}

static V3 rotate_y(V3 p, float a)
{
    float c = cosf(a);
    float s = sinf(a);
    V3 out = {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
    return out;
}

static V3 vec3_add(V3 a, V3 b)
{
    V3 out = {a.x + b.x, a.y + b.y, a.z + b.z};
    return out;
}

static V3 vec3_scale(V3 v, float s)
{
    V3 out = {v.x * s, v.y * s, v.z * s};
    return out;
}

static float vec3_len(V3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static V3 vec3_normalize(V3 v)
{
    float len = vec3_len(v);
    if (len <= 1e-6f)
        return (V3){0.0f, 0.0f, 0.0f};
    return vec3_scale(v, 1.0f / len);
}

static V3 camera_forward_flat(const Camera *cam)
{
    V3 out = {-sinf(cam->yaw), 0.0f, -cosf(cam->yaw)};
    return out;
}

static V3 camera_right(const Camera *cam)
{
    V3 out = {cosf(cam->yaw), 0.0f, -sinf(cam->yaw)};
    return out;
}

static V3 world_to_view(V3 p, const Demo *demo)
{
    V3 v = {
        p.x - demo->cam.pos.x,
        p.y - demo->cam.pos.y,
        p.z - demo->cam.pos.z,
    };

    v = rotate_y(v, -demo->cam.yaw);
    v = rotate_x(v, -demo->cam.pitch);
    return v;
}

static int project_clip_point(ClipVert in, GrVertex *out)
{
    if (in.w <= VIEW_NEAR_Z)
        return 0;

    out->x = in.x;
    out->y = in.y;
    out->z = in.z;
    out->oow = 1.0f / in.w;
    out->r = in.r;
    out->g = in.g;
    out->b = in.b;
    out->a = in.a;
    out->u = in.u;
    out->v = in.v;
    return 1;
}

static ClipVert make_clip_vert(V3 view, GrColor_t color, float alpha, float u, float v)
{
    const float f = 1.35f;
    const float aspect = FB_W / FB_H;
    ClipVert out;
    grColorUnpack(color, &out.r, &out.g, &out.b, &out.a);
    out.x = view.x * f / aspect;
    out.y = view.y * f;
    out.z = ((VIEW_FAR_Z + VIEW_NEAR_Z) / (VIEW_NEAR_Z - VIEW_FAR_Z)) * view.z
          + ((2.0f * VIEW_FAR_Z * VIEW_NEAR_Z) / (VIEW_NEAR_Z - VIEW_FAR_Z));
    out.w = -view.z;
    out.a = alpha;
    out.u = u;
    out.v = v;
    return out;
}

static ClipVert clip_lerp(ClipVert a, ClipVert b, float t)
{
    ClipVert out;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.w = a.w + (b.w - a.w) * t;
    out.r = a.r + (b.r - a.r) * t;
    out.g = a.g + (b.g - a.g) * t;
    out.b = a.b + (b.b - a.b) * t;
    out.a = a.a + (b.a - a.a) * t;
    out.u = a.u + (b.u - a.u) * t;
    out.v = a.v + (b.v - a.v) * t;
    return out;
}

static float clip_plane_eval(ClipVert v, int plane)
{
    switch (plane) {
        case 0: return v.x + v.w;
        case 1: return v.w - v.x;
        case 2: return v.y + v.w;
        case 3: return v.w - v.y;
        case 4: return v.z + v.w;
        default: return v.w - v.z;
    }
}

static int draw_clip_triangle_clipped(ClipVert a, ClipVert b, ClipVert c)
{
    ClipVert poly_a[12];
    ClipVert poly_b[12];
    ClipVert *src = poly_a;
    ClipVert *dst = poly_b;
    int src_count = 3;

    src[0] = a;
    src[1] = b;
    src[2] = c;

    for (int plane = 0; plane < 6; plane++) {
        int dst_count = 0;

        for (int i = 0; i < src_count; i++) {
            ClipVert cur = src[i];
            ClipVert prev = src[(i + src_count - 1) % src_count];
            float cur_d = clip_plane_eval(cur, plane);
            float prev_d = clip_plane_eval(prev, plane);
            int cur_in = cur_d >= 0.0f;
            int prev_in = prev_d >= 0.0f;

            if (cur_in != prev_in) {
                float t = prev_d / (prev_d - cur_d);
                dst[dst_count++] = clip_lerp(prev, cur, t);
            }
            if (cur_in)
                dst[dst_count++] = cur;
        }

        if (dst_count < 3)
            return 0;

        {
            ClipVert *tmp = src;
            src = dst;
            dst = tmp;
            src_count = dst_count;
        }
    }

    for (int i = 1; i + 1 < src_count; i++) {
        GrVertex v0, v1, v2;
        if (!project_clip_point(src[0], &v0))
            continue;
        if (!project_clip_point(src[i], &v1))
            continue;
        if (!project_clip_point(src[i + 1], &v2))
            continue;
        grDrawTriangle(&v0, &v1, &v2);
    }

    return 1;
}

static void draw_rect_z(int x, int y, int w, int h, float z, GrColor_t color, float alpha)
{
    GrVertex v0 = v2d((float)x, (float)y, z, color, alpha, 0.0f, 0.0f);
    GrVertex v1 = v2d((float)(x + w), (float)y, z, color, alpha, 1.0f, 0.0f);
    GrVertex v2 = v2d((float)x, (float)(y + h), z, color, alpha, 0.0f, 1.0f);
    GrVertex v3 = v2d((float)(x + w), (float)(y + h), z, color, alpha, 1.0f, 1.0f);
    grDrawTriangle(&v0, &v1, &v2);
    grDrawTriangle(&v2, &v1, &v3);
}

static void draw_rect(int x, int y, int w, int h, GrColor_t color, float alpha)
{
    draw_rect_z(x, y, w, h, -0.96f, color, alpha);
}

static void draw_textured_rect_z(int x, int y, int w, int h, float z, int tex, GrColor_t color, float alpha)
{
    GrVertex v0 = v2d((float)x, (float)y, z, color, alpha, 0.0f, 0.0f);
    GrVertex v1 = v2d((float)(x + w), (float)y, z, color, alpha, 1.0f, 0.0f);
    GrVertex v2 = v2d((float)x, (float)(y + h), z, color, alpha, 0.0f, 1.0f);
    GrVertex v3 = v2d((float)(x + w), (float)(y + h), z, color, alpha, 1.0f, 1.0f);
    grTexBind(tex);
    grDrawTriangle(&v0, &v1, &v2);
    grDrawTriangle(&v2, &v1, &v3);
}

static void draw_textured_rect(int x, int y, int w, int h, int tex, GrColor_t color, float alpha)
{
    draw_textured_rect_z(x, y, w, h, -0.94f, tex, color, alpha);
}

static void draw_background(void)
{
    for (int y = 0; y < 240; y += 6) {
        float k = (float)y / 239.0f;
        GrColor_t color = grColorPack(0.06f + 0.18f * k, 0.05f + 0.12f * k, 0.14f + 0.34f * k, 1.0f);
        draw_rect_z(0, y, 320, 6, 0.995f, color, 1.0f);
    }

    grAlphaBlend(GR_BLEND_ADD);
    grTexBind(-1);
    draw_rect_z(118, 172, 84, 30, 0.992f, 0xFF88B8FF, 0.12f);
    draw_rect_z(128, 180, 64, 12, 0.991f, 0xFFFFD080, 0.18f);
    grAlphaBlend(GR_BLEND_NONE);
}

static void draw_floor(const Demo *demo, int tex)
{
    const int half_w = 10;
    const int depth = 16;
    const float tile = 1.5f;
    const float origin_z = 0.0f;

    grTexBind(tex);
    grColorCombine(GR_COMBINE_MODE_MODULATE);
    grAlphaCombine(GR_COMBINE_MODE_MODULATE);

    for (int z = 0; z < depth; z++) {
        for (int x = -half_w; x < half_w; x++) {
            float x0 = (float)x * tile;
            float x1 = x0 + tile;
            float z1 = origin_z - (float)(z + 1) * tile;
            float z0 = z1 + tile;
            V3 p0 = world_to_view((V3){x0, -1.0f, z0}, demo);
            V3 p1 = world_to_view((V3){x1, -1.0f, z0}, demo);
            V3 p2 = world_to_view((V3){x0, -1.0f, z1}, demo);
            V3 p3 = world_to_view((V3){x1, -1.0f, z1}, demo);

            ClipVert v0 = make_clip_vert(p0, 0xFFE0D8C0, 1.0f, 0.0f, 0.0f);
            ClipVert v1 = make_clip_vert(p1, 0xFFE0D8C0, 1.0f, 1.0f, 0.0f);
            ClipVert v2 = make_clip_vert(p2, 0xFFE0D8C0, 1.0f, 0.0f, 1.0f);
            ClipVert v3 = make_clip_vert(p3, 0xFFE0D8C0, 1.0f, 1.0f, 1.0f);
            draw_clip_triangle_clipped(v0, v1, v2);
            draw_clip_triangle_clipped(v2, v1, v3);
        }
    }
}

static void draw_cube(const Demo *demo, V3 center, V3 scale, float rx, float ry, int tex, GrColor_t tint)
{
    grTexBind(tex);

    for (int face = 0; face < 6; face++) {
        for (int tri = 0; tri < 2; tri++) {
            int ids[3] = {
                kCubeFaces[face][0],
                kCubeFaces[face][tri + 1],
                kCubeFaces[face][tri + 2],
            };
            ClipVert vv[3];

            for (int i = 0; i < 3; i++) {
                V3 p = kCubeVerts[ids[i]];
                int uv_idx = i == 0 ? 0 : (tri + i);
                p.x *= scale.x;
                p.y *= scale.y;
                p.z *= scale.z;
                p = rotate_x(p, rx);
                p = rotate_y(p, ry);
                p.x += center.x;
                p.y += center.y;
                p.z += center.z;
                vv[i] = make_clip_vert(world_to_view(p, demo), tint, 1.0f,
                                       kQuadUV[uv_idx][0], kQuadUV[uv_idx][1]);
            }

            draw_clip_triangle_clipped(vv[0], vv[1], vv[2]);
        }
    }
}

static void draw_billboard(const Demo *demo, V3 center, float size, int tex, GrColor_t tint, float alpha)
{
    V3 center_view = world_to_view(center, demo);
    V3 right = vec3_scale(camera_right(&demo->cam), size);
    V3 up = {0.0f, size, 0.0f};
    V3 p0 = vec3_add(vec3_add(center, vec3_scale(right, -1.0f)), vec3_scale(up, -1.0f));
    V3 p1 = vec3_add(vec3_add(center, right), vec3_scale(up, -1.0f));
    V3 p2 = vec3_add(vec3_add(center, vec3_scale(right, -1.0f)), up);
    V3 p3 = vec3_add(vec3_add(center, right), up);

    if (-center_view.z < size * 1.4f)
        return;

    ClipVert v0 = make_clip_vert(world_to_view(p0, demo), tint, alpha, 0.0f, 0.0f);
    ClipVert v1 = make_clip_vert(world_to_view(p1, demo), tint, alpha, 1.0f, 0.0f);
    ClipVert v2 = make_clip_vert(world_to_view(p2, demo), tint, alpha, 0.0f, 1.0f);
    ClipVert v3 = make_clip_vert(world_to_view(p3, demo), tint, alpha, 1.0f, 1.0f);

    grTexBind(tex);
    draw_clip_triangle_clipped(v0, v1, v2);
    draw_clip_triangle_clipped(v2, v1, v3);
}

static void draw_starfield(float t)
{
    grTexBind(-1);
    grAlphaBlend(GR_BLEND_NONE);

    for (int i = 0; i < 28; i++) {
        float px = 20.0f + (float)((i * 37) % 280);
        float py = 138.0f + (float)((i * 19) % 40);
        float twinkle = 0.25f + 0.75f * (0.5f + 0.5f * sinf(t * 3.0f + (float)i));
        GrVertex v = v2d(px, py, 0.989f, 0xFFFFFFFF, twinkle, 0.0f, 0.0f);
        grDrawPoint(&v);
    }
}

static void draw_scene(float t, const Demo *demo)
{
    draw_background();
    draw_starfield(t);

    if (demo->fog_on) {
        grFogMode(GR_FOG_LINEAR);
        grFogColorValue(0xFF25182E);
        grFogRange(4.5f, 11.0f);
    } else {
        grFogMode(GR_FOG_DISABLE);
    }

    grCullMode(demo->cull_on ? GR_CULL_POSITIVE : GR_CULL_DISABLE);
    grAlphaBlend(GR_BLEND_NONE);
    grAlphaTest(GR_ALPHATEST_DISABLE, 0.0f);
    draw_floor(demo, demo->tex_checker);

    draw_cube(demo, (V3){-1.8f, -0.10f, -4.8f}, (V3){1.2f, 1.2f, 1.2f},
              DEG2RAD(18.0f) + t * 0.5f, t * 0.7f,
              demo->tex_checker, 0xFFFFFFFF);
    draw_cube(demo, (V3){ 1.9f, -0.10f, -7.0f}, (V3){1.0f, 2.2f, 1.0f},
              0.0f, -t * 0.35f,
              demo->tex_rgb565, 0xFFE8F4FF);
    draw_cube(demo, (V3){ 0.0f, -0.55f, -9.5f}, (V3){0.9f, 0.9f, 0.9f},
              t * 0.2f, t * 0.25f,
              demo->tex_gradient, 0xFFB8A8FF);

    if (demo->alpha_test_on) {
        grAlphaTest(GR_ALPHATEST_GT, 0.5f);
        draw_billboard(demo, (V3){0.0f, 1.0f, -7.4f}, 0.9f, demo->tex_ring, 0xFFFFFFFF, 1.0f);
        grAlphaTest(GR_ALPHATEST_DISABLE, 0.0f);
    }

    if (demo->blend_on) {
        grAlphaBlend(GR_BLEND_ALPHA);
        draw_billboard(demo, (V3){0.0f, 0.35f, -8.2f}, 1.0f, demo->tex_gradient, 0xFFA0E8FF, 0.18f);
        grAlphaBlend(GR_BLEND_ADD);
        draw_billboard(demo, (V3){0.0f, 0.55f, -7.8f}, 0.8f, demo->tex_gradient, 0xFFFFFFFF, 0.08f);
        grAlphaBlend(GR_BLEND_NONE);
    }

    grFogMode(GR_FOG_DISABLE);
    grCullMode(GR_CULL_DISABLE);
}

static void draw_hud(const Demo *demo, float t)
{
    float pulse = 0.5f + 0.5f * sinf(t * 2.5f);

    grAlphaBlend(GR_BLEND_ALPHA);
    grTexBind(-1);
    draw_rect(10, 10, 132, 14, 0xFF090B12, 0.52f);
    draw_rect(10, 198, 200, 24, 0xFF090B12, 0.52f);

    draw_rect(14, 13, 22, 8, 0xFF2B4A72, 0.80f);
    draw_rect(40, 13, 22, 8, 0xFF6A4B2C, 0.80f);
    draw_rect(66, 13, 22, 8, 0xFF2D5A3C, 0.80f);
    draw_rect(92, 13, 22, 8, 0xFF66426F, 0.80f);

    draw_textured_rect(16, 201, 18, 18, demo->tex_checker, 0xFFFFFFFF, 1.0f);
    draw_textured_rect(40, 201, 18, 18, demo->tex_rgb565, 0xFFFFFFFF, 1.0f);
    draw_textured_rect(64, 201, 18, 18, demo->tex_gradient, 0xFFFFFFFF, 1.0f);

    draw_textured_rect(118, 201, 24, 18, demo->tex_gradient, 0xFFFFFFFF, 1.0f);
    if (demo->alpha_test_on) {
        grAlphaTest(GR_ALPHATEST_GT, 0.5f);
        draw_textured_rect(148, 198, 22, 22, demo->tex_ring, 0xFFFFFFFF, 1.0f);
        grAlphaTest(GR_ALPHATEST_DISABLE, 0.0f);
    }
    if (demo->blend_on) {
        grAlphaBlend(GR_BLEND_ADD);
        draw_textured_rect(176, 201, 18, 18, demo->tex_gradient,
                           grColorPack(1.0f, 0.8f + 0.2f * pulse, 0.6f, 1.0f), 0.55f);
    }
    grAlphaBlend(GR_BLEND_NONE);

    if (demo->cam.freelook) {
        float cx = 160.0f;
        float cy = 118.0f;
        GrVertex h0 = v2d(cx - 6.0f, cy, -0.91f, 0xFFFFFFFF, 0.4f, 0.0f, 0.0f);
        GrVertex h1 = v2d(cx + 6.0f, cy, -0.91f, 0xFFFFFFFF, 0.4f, 0.0f, 0.0f);
        GrVertex v0 = v2d(cx, cy - 6.0f, -0.91f, 0xFFFFFFFF, 0.4f, 0.0f, 0.0f);
        GrVertex v1 = v2d(cx, cy + 6.0f, -0.91f, 0xFFFFFFFF, 0.4f, 0.0f, 0.0f);
        grDrawLine(&h0, &h1);
        grDrawLine(&v0, &v1);
    }
}

static void update_title(GLFWwindow *window, Demo *demo, int fw, int fh)
{
    static const char *kFilterNames[3] = {"POINT", "LINEAR", "MIPMAP"};
    double now = glfwGetTime();
    demo->frames++;

    if (now - demo->last_title >= 1.0) {
        char title[256];
        snprintf(title, sizeof(title),
                 "glint | %d FPS | RMB freelook WASDQE move Shift fast | 1 filter=%s 2 fog=%s 3 blend=%s 4 atest=%s 5 cull=%s | %dx%d",
                 demo->frames,
                 kFilterNames[demo->filter_mode],
                 demo->fog_on ? "on" : "off",
                 demo->blend_on ? "on" : "off",
                 demo->alpha_test_on ? "on" : "off",
                 demo->cull_on ? "on" : "off",
                 fw, fh);
        glfwSetWindowTitle(window, title);
        demo->frames = 0;
        demo->last_title = now;
    }
}

static void set_freelook(GLFWwindow *window, Demo *demo, int enabled)
{
    demo->cam.freelook = enabled;
    demo->cam.mouse_seeded = 0;
    glfwSetInputMode(window, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

static void update_camera(GLFWwindow *window, Demo *demo, float dt)
{
    Camera *cam = &demo->cam;
    V3 move = {0.0f, 0.0f, 0.0f};
    float speed = 3.0f;
    V3 forward;
    V3 right;

    if (!cam->freelook)
        return;

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= 3.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        speed *= 0.35f;

    {
        double mx = 0.0;
        double my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        if (!cam->mouse_seeded) {
            cam->last_mouse_x = mx;
            cam->last_mouse_y = my;
            cam->mouse_seeded = 1;
        } else {
            float dx = (float)(mx - cam->last_mouse_x);
            float dy = (float)(my - cam->last_mouse_y);
            cam->last_mouse_x = mx;
            cam->last_mouse_y = my;
            cam->yaw -= dx * 0.0035f;
            cam->pitch -= dy * 0.0030f;
            if (cam->pitch > 1.45f) cam->pitch = 1.45f;
            if (cam->pitch < -1.45f) cam->pitch = -1.45f;
        }
    }

    forward = camera_forward_flat(cam);
    right = camera_right(cam);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        move = vec3_add(move, forward);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        move = vec3_add(move, vec3_scale(forward, -1.0f));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        move = vec3_add(move, right);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        move = vec3_add(move, vec3_scale(right, -1.0f));
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        move.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        move.y -= 1.0f;

    move = vec3_normalize(move);
    cam->pos = vec3_add(cam->pos, vec3_scale(move, dt * speed));
}

static void key_cb(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    Demo *demo = (Demo *)glfwGetWindowUserPointer(window);
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS)
        return;

    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (demo->cam.freelook)
                set_freelook(window, demo, 0);
            else
                glfwSetWindowShouldClose(window, 1);
            break;
        case GLFW_KEY_1:
            demo->filter_mode = (demo->filter_mode + 1) % 3;
            apply_filter_mode(demo);
            break;
        case GLFW_KEY_2:
            demo->fog_on = !demo->fog_on;
            break;
        case GLFW_KEY_3:
            demo->blend_on = !demo->blend_on;
            break;
        case GLFW_KEY_4:
            demo->alpha_test_on = !demo->alpha_test_on;
            break;
        case GLFW_KEY_5:
            demo->cull_on = !demo->cull_on;
            break;
    }
}

static void mouse_button_cb(GLFWwindow *window, int button, int action, int mods)
{
    Demo *demo = (Demo *)glfwGetWindowUserPointer(window);
    (void)mods;

    if (button != GLFW_MOUSE_BUTTON_RIGHT)
        return;

    if (action == GLFW_PRESS)
        set_freelook(window, demo, 1);
    else if (action == GLFW_RELEASE)
        set_freelook(window, demo, 0);
}

int main(void)
{
    GLFWwindow *window;
    Demo demo = {0};

    test_pocketpy();
    test_miniaudio();

    glfwSetErrorCallback(error_cb);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(960, 720,
                              "glint | RMB freelook WASDQE move | 1 filter 2 fog 3 blend 4 atest 5 cull",
                              NULL, NULL);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, &demo);
    glfwSetKeyCallback(window, key_cb);
    glfwSetMouseButtonCallback(window, mouse_button_cb);
    glfwSwapInterval(1);

    if (!gladLoadGLES2(glad_loader)) {
        fprintf(stderr, "gladLoadGLES2 failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (!grInit(960, 720)) {
        fprintf(stderr, "grInit failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    demo.tex_checker = grTexAllocate();
    demo.tex_gradient = grTexAllocate();
    demo.tex_ring = grTexAllocate();
    demo.tex_rgb565 = grTexAllocate();
    demo.blend_on = 1;
    demo.alpha_test_on = 1;
    demo.fog_on = 1;
    demo.cull_on = 0;
    demo.cam.pos = (V3){0.0f, 0.6f, 2.5f};
    demo.cam.yaw = 0.0f;
    demo.cam.pitch = DEG2RAD(-8.0f);
    demo.last_frame = glfwGetTime();

    if (demo.tex_checker >= 0) {
        uint8_t *pixels = make_checker_rgba(64, 64);
        grTexDownloadMipMap(demo.tex_checker, pixels, 64, 64, GR_TEXFMT_ARGB_8888);
        free(pixels);
    }
    if (demo.tex_gradient >= 0) {
        uint8_t *pixels = make_gradient_rgba(64, 64);
        grTexDownloadMipMap(demo.tex_gradient, pixels, 64, 64, GR_TEXFMT_ARGB_8888);
        free(pixels);
    }
    if (demo.tex_ring >= 0) {
        uint8_t *pixels = make_ring_rgba(64, 64);
        grTexDownloadMipMap(demo.tex_ring, pixels, 64, 64, GR_TEXFMT_ARGB_8888);
        free(pixels);
    }
    if (demo.tex_rgb565 >= 0) {
        uint16_t *pixels = make_rgb565(64, 64);
        grTexDownloadMipMap(demo.tex_rgb565, pixels, 64, 64, GR_TEXFMT_RGB_565);
        free(pixels);
    }

    apply_filter_mode(&demo);

    while (!glfwWindowShouldClose(window)) {
        int fw = 0;
        int fh = 0;
        float t = (float)glfwGetTime();
        float dt = (float)(glfwGetTime() - demo.last_frame);
        if (dt > 0.1f) dt = 0.1f;
        demo.last_frame = glfwGetTime();

        glfwGetFramebufferSize(window, &fw, &fh);
        update_camera(window, &demo, dt);
        grViewport(0, 0, 320, 240);
        grBufferClear(0xFF140C18, 0xFFFF);

        draw_scene(t, &demo);
        draw_hud(&demo, t);
        grBufferSwap(window);

        update_title(window, &demo, fw, fh);
        glfwPollEvents();
    }

    grTexFree(demo.tex_checker);
    grTexFree(demo.tex_gradient);
    grTexFree(demo.tex_ring);
    grTexFree(demo.tex_rgb565);
    grShutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
