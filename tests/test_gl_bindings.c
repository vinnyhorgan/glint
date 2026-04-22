#include "gl.h"
#include "gl_core.h"
#include "gl_bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int tex;
    int width;
    int height;
    GrTextureFormat fmt;
    uint8_t pixels[64];
    int pixel_bytes;
} TextureUploadRecord;

static int g_failures;
static GrColor_t g_last_clear_color;
static GrAlpha_t g_last_clear_alpha;
static GrDepth_t g_last_clear_depth;
static GrVertex g_first_triangle[3];
static GrVertex g_last_triangle[3];
static int g_triangle_count;
static GrVertex g_last_point;
static int g_point_count;
static GrVertex g_last_line[2];
static int g_line_count;
static TextureUploadRecord g_tex_upload;
static int g_next_tex = 1;
static int g_last_tex_filter_tex;
static GrMipMapMode g_last_tex_filter_mm;
static GrTextureFilter g_last_tex_filter_min;
static GrTextureFilter g_last_tex_filter_mag;
static int g_last_tex_clamp_tex;
static GrTextureClampMode g_last_tex_clamp_s;
static GrTextureClampMode g_last_tex_clamp_t;
static GrCombineFunction_t g_last_color_func;
static GrCombineFactor_t g_last_color_factor;
static GrCombineLocal_t g_last_color_local;
static GrCombineOther_t g_last_color_other;
static bool g_last_color_invert;
static GrCombineFunction_t g_last_alpha_func;
static GrCombineFactor_t g_last_alpha_factor;
static GrCombineLocal_t g_last_alpha_local;
static GrCombineOther_t g_last_alpha_other;
static bool g_last_alpha_invert;
static GrAlphaBlendFnc_t g_last_rgb_sf;
static GrAlphaBlendFnc_t g_last_rgb_df;
static GrAlphaBlendFnc_t g_last_alpha_sf;
static GrAlphaBlendFnc_t g_last_alpha_df;
static GrDepthBufferMode_t g_last_depth_mode;
static bool g_last_depth_mask;
static int g_last_bound_tex;
static double g_host_time = 12.5;
static double g_host_dt = 0.25;
static char g_last_title[128];
static int g_quit_requested;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
    } \
} while (0)

static int nearf(float a, float b)
{
    float d = a - b;
    return d < 0.0f ? -d < 1e-4f : d < 1e-4f;
}

static double test_host_time_now(void *userdata)
{
    (void)userdata;
    return g_host_time;
}

static double test_host_delta_time(void *userdata)
{
    (void)userdata;
    return g_host_dt;
}

static int test_host_key_down(void *userdata, int key)
{
    (void)userdata;
    return key == 263 || key == 'A';
}

static int test_host_key_pressed(void *userdata, int key)
{
    (void)userdata;
    return key == 32;
}

static int test_host_mouse_down(void *userdata, int button)
{
    (void)userdata;
    return button == 1;
}

static int test_host_mouse_pressed(void *userdata, int button)
{
    (void)userdata;
    return button == 0;
}

static int test_host_mouse_released(void *userdata, int button)
{
    (void)userdata;
    return button == 2;
}

static void test_host_mouse_position(void *userdata, float *x, float *y)
{
    (void)userdata;
    *x = 123.0f;
    *y = 45.0f;
}

static void test_host_framebuffer_size(void *userdata, int *w, int *h)
{
    (void)userdata;
    *w = 960;
    *h = 720;
}

static void test_host_set_title(void *userdata, const char *title)
{
    (void)userdata;
    snprintf(g_last_title, sizeof(g_last_title), "%s", title);
}

static void test_host_request_quit(void *userdata)
{
    (void)userdata;
    g_quit_requested = 1;
}

int grInit(int win_w, int win_h)
{
    (void)win_w;
    (void)win_h;
    return 1;
}

void grShutdown(void) {}

void grBufferClear(GrColor_t color, GrAlpha_t alpha, GrDepth_t depth)
{
    g_last_clear_color = color;
    g_last_clear_alpha = alpha;
    g_last_clear_depth = depth;
}

void grBufferSwap(struct GLFWwindow *window) { (void)window; }
void grBufferSwapCurrent(void) {}
void grViewport(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
void grClipWindow(int xmin, int ymin, int xmax, int ymax)
{
    (void)xmin;
    (void)ymin;
    (void)xmax;
    (void)ymax;
}

void grColorCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert)
{
    g_last_color_func = func;
    g_last_color_factor = factor;
    g_last_color_local = local;
    g_last_color_other = other;
    g_last_color_invert = invert;
}

void grAlphaCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert)
{
    g_last_alpha_func = func;
    g_last_alpha_factor = factor;
    g_last_alpha_local = local;
    g_last_alpha_other = other;
    g_last_alpha_invert = invert;
}

void grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf,
                          GrAlphaBlendFnc_t rgb_df,
                          GrAlphaBlendFnc_t alpha_sf,
                          GrAlphaBlendFnc_t alpha_df)
{
    g_last_rgb_sf = rgb_sf;
    g_last_rgb_df = rgb_df;
    g_last_alpha_sf = alpha_sf;
    g_last_alpha_df = alpha_df;
}

void grAlphaTestFunction(GrCmpFnc_t func) { (void)func; }
void grAlphaTestReferenceValue(GrAlpha_t value) { (void)value; }
void grConstantColorValue(GrColor_t color) { (void)color; }
void grDepthBufferMode(GrDepthBufferMode_t mode) { g_last_depth_mode = mode; }
void grDepthBufferFunction(GrCmpFnc_t func) { (void)func; }
void grDepthMask(bool enabled) { g_last_depth_mask = enabled; }
void grCullMode(GrCullMode_t mode) { (void)mode; }
void grShadeModel(GrShadeModel_t mode) { (void)mode; }
void grFogMode(GrFogMode_t mode) { (void)mode; }
void grFogColorValue(GrColor_t color) { (void)color; }
void grFogTable(const GrFog_t table[GR_FOG_TABLE_SIZE]) { (void)table; }

int grTexAllocate(void)
{
    return g_next_tex++;
}

void grTexFree(int tex) { (void)tex; }

void grTexDownloadMipMap(int tex, const void *data, int w, int h, GrTextureFormat fmt)
{
    int bytes_per_pixel = fmt == GR_TEXFMT_ARGB_8888 ? 4 : 2;
    int total_bytes = w * h * bytes_per_pixel;
    g_tex_upload.tex = tex;
    g_tex_upload.width = w;
    g_tex_upload.height = h;
    g_tex_upload.fmt = fmt;
    g_tex_upload.pixel_bytes = total_bytes;
    memcpy(g_tex_upload.pixels, data, (size_t)total_bytes);
}

void grTexBind(int tex) { g_last_bound_tex = tex; }

void grTexFilter(int tex, GrMipMapMode mm, GrTextureFilter minf, GrTextureFilter magf)
{
    g_last_tex_filter_tex = tex;
    g_last_tex_filter_mm = mm;
    g_last_tex_filter_min = minf;
    g_last_tex_filter_mag = magf;
}

void grTexClampMode(int tex, GrTextureClampMode s_clamp, GrTextureClampMode t_clamp)
{
    g_last_tex_clamp_tex = tex;
    g_last_tex_clamp_s = s_clamp;
    g_last_tex_clamp_t = t_clamp;
}

void grDrawTriangle(const GrVertex *v0, const GrVertex *v1, const GrVertex *v2)
{
    if (g_triangle_count == 0) {
        g_first_triangle[0] = *v0;
        g_first_triangle[1] = *v1;
        g_first_triangle[2] = *v2;
    }
    g_last_triangle[0] = *v0;
    g_last_triangle[1] = *v1;
    g_last_triangle[2] = *v2;
    g_triangle_count++;
}

void grDrawPoint(const GrVertex *v)
{
    g_last_point = *v;
    g_point_count++;
}

void grDrawLine(const GrVertex *v0, const GrVertex *v1)
{
    g_last_line[0] = *v0;
    g_last_line[1] = *v1;
    g_line_count++;
}

GrColor_t grColorPack(float r, float g, float b, float a)
{
    return ((uint32_t)grCoreFloatToByte(a) << 24)
         | ((uint32_t)grCoreFloatToByte(b) << 16)
         | ((uint32_t)grCoreFloatToByte(g) << 8)
         | (uint32_t)grCoreFloatToByte(r);
}

void grColorUnpack(GrColor_t c, float *r, float *g, float *b, float *a)
{
    if (r) *r = (float)(c & 0xFFu) / 255.0f;
    if (g) *g = (float)((c >> 8) & 0xFFu) / 255.0f;
    if (b) *b = (float)((c >> 16) & 0xFFu) / 255.0f;
    if (a) *a = (float)((c >> 24) & 0xFFu) / 255.0f;
}

int run_gl_bindings_tests(void)
{
    py_GlobalRef glide;
    py_GlobalRef main_mod;
    GlBindingHost host = {0};

    g_failures = 0;
    py_initialize();

    host.time_now = test_host_time_now;
    host.delta_time = test_host_delta_time;
    host.key_down = test_host_key_down;
    host.key_pressed = test_host_key_pressed;
    host.mouse_down = test_host_mouse_down;
    host.mouse_pressed = test_host_mouse_pressed;
    host.mouse_released = test_host_mouse_released;
    host.mouse_position = test_host_mouse_position;
    host.framebuffer_size = test_host_framebuffer_size;
    host.set_title = test_host_set_title;
    host.request_quit = test_host_request_quit;
    glBindingsSetHost(&host);

    glide = py_newmodule("glide");
    CHECK(glBindingsRegister(glide));
    if (py_checkexc()) {
        py_printexc();
        g_failures++;
    }

    main_mod = py_getmodule("__main__");
    if (!py_exec(
            "import glide\n"
            "packed = glide.rgba(1.0, 0.5, 0.25, 0.5)\n"
            "assert glide.FOG_TABLE_SIZE == 64\n"
            "assert glide.time() == 12.5\n"
            "assert glide.dt() == 0.25\n"
            "assert glide.key_down('left')\n"
            "assert glide.key_down('a')\n"
            "assert glide.key_pressed('space')\n"
            "assert glide.mouse_down('right')\n"
            "assert glide.mouse_pressed('left')\n"
            "assert glide.mouse_released('middle')\n"
            "assert glide.mouse_position() == (123.0, 45.0)\n"
            "assert glide.mouse_x() == 123\n"
            "assert glide.mouse_y() == 45\n"
            "assert isinstance(glide.mouse_x(), int)\n"
            "assert isinstance(glide.mouse_y(), int)\n"
            "assert glide.screen_size() == (320, 240)\n"
            "assert glide.color_tuple(packed)[0] == 1.0\n"
            "v0 = glide.vertex(10.0, 20.0, 0.25, 0.5, packed, 0.75, 0.125)\n"
            "v1 = v0.copy()\n"
            "v2 = glide.Vertex(30.0, 40.0, 0.75, 1.0, (0.2, 0.3, 0.4, 1.0), 0.0, 1.0)\n"
            "glide.triangle(v0, v1, v2)\n"
            "glide.point(v0)\n"
            "glide.line(v1, v2)\n"
            "glide.rect(50.0, 60.0, 20.0, 10.0, (0.9, 0.2, 0.1, 1.0))\n"
            "fog = glide.make_fog_table(2.0, 10.0)\n"
            "assert len(fog) == glide.FOG_TABLE_SIZE\n"
            "assert fog[0] == 0\n"
            "glide.triangle((1.0, 2.0), (3.0, 4.0, 0.25, 0.5), (5.0, 6.0, 0.5, 0.25, 0.125))\n"
             "glide.triangle((7.0, 8.0, 1.0, 0.5, 0.0, 0.25, 0.75), (9.0, 10.0, 0.5, 1.0, 1.0, 1.0, 1.0, 0.0), v0)\n"
            "tex = glide.upload_texture(2, 1, [128, 1, 2, 3, 255, 4, 5, 6])\n"
            "assert tex == 1\n"
            "glide.set_textured_modulate()\n"
"glide.image(tex, 100.0, 120.0, 16.0, 12.0, (0.8, 0.7, 0.6, 0.5))\n"
            "glide.set_mode('flat')\n"
            "glide.set_mode('gouraud')\n"
            "glide.set_mode('textured')\n"
            "glide.set_mode('textured_gouraud')\n"
            "glide.set_mode('transparent')\n"
            "glide.begin_2d()\n"
            "glide.begin_3d()\n"
            "try:\n"
            "    glide.tex_download_mipmap(1, 1, 1, 999, [0, 0, 0, 0])\n"
            "    raise AssertionError('expected invalid texture format failure')\n"
            "except ValueError:\n"
            "    pass\n"
            "glide.clear((0.25, 0.5, 0.75, 0.5), 0x1234)\n"
            "glide.set_title('binding test')\n"
            "glide.quit()\n"
            "glide.set_untextured()\n"
            "glide.set_textured_modulate()\n"
            "glide.set_blend_none()\n"
            "glide.set_blend_alpha()\n"
            "glide.set_blend_add()\n"
"glide.buffer_swap()\n",
            "<bindings-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }

    CHECK(g_triangle_count == 7);
    CHECK(g_point_count == 1);
    CHECK(g_line_count == 1);
    CHECK(nearf(g_first_triangle[0].x, 10.0f));
    CHECK(nearf(g_first_triangle[0].y, 20.0f));
    CHECK(nearf(g_first_triangle[0].z, 0.25f));
    CHECK(nearf(g_first_triangle[0].oow, 0.5f));
    CHECK(nearf(g_first_triangle[0].u, 0.75f));
    CHECK(nearf(g_first_triangle[0].v, 0.125f));
    CHECK(nearf(g_first_triangle[2].r, 0.2f));
    CHECK(nearf(g_first_triangle[2].g, 0.3f));
    CHECK(nearf(g_first_triangle[2].b, 0.4f));
    CHECK(nearf(g_first_triangle[2].a, 1.0f));
    CHECK(nearf(g_last_triangle[0].x, 100.0f));
    CHECK(nearf(g_last_triangle[0].u, 0.0f));
    CHECK(nearf(g_last_triangle[0].a, 0.5f));

    CHECK(g_tex_upload.tex == 1);
    CHECK(g_tex_upload.width == 2);
    CHECK(g_tex_upload.height == 1);
    CHECK(g_tex_upload.fmt == GR_TEXFMT_ARGB_8888);
    CHECK(g_tex_upload.pixel_bytes == 8);
    CHECK(g_tex_upload.pixels[0] == 128);
    CHECK(g_tex_upload.pixels[7] == 6);
    CHECK(g_last_tex_filter_tex == 1);
    CHECK(g_last_tex_filter_mm == GR_MIPMAP_DISABLE);
    CHECK(g_last_tex_filter_min == GR_TEXTUREFILTER_POINT_SAMPLED);
    CHECK(g_last_tex_filter_mag == GR_TEXTUREFILTER_POINT_SAMPLED);
    CHECK(g_last_tex_clamp_tex == 1);
    CHECK(g_last_tex_clamp_s == GR_TEXTURECLAMP_WRAP);
    CHECK(g_last_tex_clamp_t == GR_TEXTURECLAMP_WRAP);
    CHECK(g_last_bound_tex == -1);
    CHECK(strcmp(g_last_title, "binding test") == 0);
    CHECK(g_quit_requested == 1);

    CHECK(g_last_clear_color == grColorPack(0.25f, 0.5f, 0.75f, 0.5f));
    CHECK(g_last_clear_alpha == 128);
    CHECK(g_last_clear_depth == 0x1234);

    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_LOCAL);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_ITERATED);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_TEXTURE);
    CHECK(!g_last_color_invert);
    CHECK(g_last_alpha_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(g_last_alpha_factor == GR_COMBINE_FACTOR_LOCAL_ALPHA);
    CHECK(g_last_alpha_local == GR_COMBINE_LOCAL_ITERATED);
    CHECK(g_last_alpha_other == GR_COMBINE_OTHER_TEXTURE);
    CHECK(!g_last_alpha_invert);
    CHECK(g_last_depth_mode == GR_DEPTHBUFFER_ZBUFFER);
    CHECK(g_last_depth_mask);

    CHECK(g_last_rgb_sf == GR_BLEND_SRC_ALPHA);
    CHECK(g_last_rgb_df == GR_BLEND_ONE);
    CHECK(g_last_alpha_sf == GR_BLEND_ONE);
    CHECK(g_last_alpha_df == GR_BLEND_ONE);

    memset(&g_last_color_func, 0, sizeof(g_last_color_func));
    memset(&g_last_color_factor, 0, sizeof(g_last_color_factor));
    memset(&g_last_color_local, 0, sizeof(g_last_color_local));
    memset(&g_last_color_other, 0, sizeof(g_last_color_other));
    memset(&g_last_alpha_func, 0, sizeof(g_last_alpha_func));
    memset(&g_last_alpha_factor, 0, sizeof(g_last_alpha_factor));
    memset(&g_last_alpha_local, 0, sizeof(g_last_alpha_local));
    memset(&g_last_alpha_other, 0, sizeof(g_last_alpha_other));

    if (!py_exec(
            "glide.set_untextured()\n"
            "glide.image(1, 0.0, 0.0, 1.0, 1.0)\n",
            "<image-state-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }

    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_LOCAL);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_NONE);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_ITERATED);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_NONE);
    CHECK(g_last_alpha_func == GR_COMBINE_FUNCTION_LOCAL);
    CHECK(g_last_alpha_factor == GR_COMBINE_FACTOR_NONE);

    if (!py_exec(
            "glide.set_mode('flat')\n",
            "<set-mode-flat>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_LOCAL);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_CONSTANT);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_NONE);
    CHECK(g_last_alpha_func == GR_COMBINE_FUNCTION_LOCAL);
    CHECK(g_last_alpha_local == GR_COMBINE_LOCAL_CONSTANT);
    CHECK(g_last_rgb_sf == GR_BLEND_ONE);
    CHECK(g_last_rgb_df == GR_BLEND_ZERO);

    if (!py_exec(
            "glide.set_mode('gouraud')\n",
            "<set-mode-gouraud>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_LOCAL);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_NONE);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_ITERATED);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_NONE);

    if (!py_exec(
            "glide.set_mode('textured')\n",
            "<set-mode-textured>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_LOCAL);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_CONSTANT);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_TEXTURE);

    if (!py_exec(
            "glide.set_mode('textured_gouraud')\n",
            "<set-mode-textured-gouraud>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_LOCAL);
    CHECK(g_last_color_local == GR_COMBINE_LOCAL_ITERATED);
    CHECK(g_last_color_other == GR_COMBINE_OTHER_TEXTURE);

    if (!py_exec(
            "glide.set_mode('transparent')\n",
            "<set-mode-transparent>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(g_last_color_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(g_last_color_factor == GR_COMBINE_FACTOR_LOCAL);
    CHECK(g_last_rgb_sf == GR_BLEND_SRC_ALPHA);
    CHECK(g_last_rgb_df == GR_BLEND_ONE_MINUS_SRC_ALPHA);

    if (!py_exec(
            "glide.triangle(glide.Vertex(1.0, 2.0, 0.0, 1.0, (0.5, 0.25, 0.1, 0.75)),"
            "              glide.vertex(3.0, 4.0),"
            "              glide.vertex(5.0, 6.0))\n",
            "<vertex-alpha-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(nearf(g_last_triangle[0].a, 0.75f));
    CHECK(nearf(g_last_triangle[1].a, 1.0f));
    CHECK(nearf(g_last_triangle[2].a, 1.0f));
    CHECK(nearf(g_last_triangle[0].r, 0.5f));

    /* Test Vertex color clamping: values outside 0-1 should be clamped */
    if (!py_exec(
            "def _near(a, b, eps=1e-4):\n"
            "    return abs(a - b) < eps\n"
            "v = glide.Vertex(1.0, 2.0, color=(1.5, -0.2, 0.5, 2.0))\n"
            "assert _near(v.r, 1.0), v.r\n"
            "assert _near(v.g, 0.0), v.g\n"
            "assert _near(v.b, 0.5), v.b\n"
            "assert _near(v.a, 1.0), v.a\n"
            "v2 = glide.Vertex(3.0, 4.0, color=(0.5, 0.25, 0.1, 0.75))\n"
            "assert _near(v2.r, 0.5), v2.r\n"
            "assert _near(v2.g, 0.25), v2.g\n"
            "assert _near(v2.b, 0.1), v2.b\n"
            "assert _near(v2.a, 0.75), v2.a\n"
            "glide.triangle(v, v2, glide.vertex(5.0, 6.0))\n",
            "<vertex-color-clamp-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }
    CHECK(nearf(g_last_triangle[0].r, 1.0f));
    CHECK(nearf(g_last_triangle[0].g, 0.0f));
    CHECK(nearf(g_last_triangle[0].b, 0.5f));
    CHECK(nearf(g_last_triangle[0].a, 1.0f));
    CHECK(nearf(g_last_triangle[1].r, 0.5f));

    /* Test new key name mappings */
    if (!py_exec(
            "assert glide.key_down('f1') == False\n"
            "assert glide.key_down('f12') == False\n"
            "assert glide.key_down('backspace') == False\n"
            "assert glide.key_down('delete') == False\n"
            "assert glide.key_down('insert') == False\n"
            "assert glide.key_down('home') == False\n"
            "assert glide.key_down('end') == False\n"
            "assert glide.key_down('pageup') == False\n"
            "assert glide.key_down('pagedown') == False\n"
            "assert glide.key_down(290) == False\n"  /* f1 keycode */
            "assert glide.key_down(259) == False\n" /* backspace keycode */
            ,
            "<key-names-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }

    /* Test color_pack clamping */
    if (!py_exec(
            "def _near(a, b, eps=1e-4):\n"
            "    return abs(a - b) < eps\n"
            "c = glide.color_pack(-0.5, 1.5, 0.5, 2.0)\n"
            "r, g, b, a = glide.color_unpack(c)\n"
            "assert _near(r, 0.0), r\n"
            "assert _near(g, 1.0), g\n"
            "assert _near(b, 128.0/255.0), b\n"
            "assert _near(a, 1.0), a\n",
            "<color-clamp-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }

    glBindingsSetHost(NULL);
    py_finalize();
    return g_failures;
}
