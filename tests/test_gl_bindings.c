#include "gl.h"
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

static uint8_t float_to_byte(float value)
{
    if (value <= 0.0f)
        return 0;
    if (value >= 1.0f)
        return 255;
    return (uint8_t)(value * 255.0f + 0.5f);
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

static int test_host_mouse_down(void *userdata, int button)
{
    (void)userdata;
    return button == 1;
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
void grDepthBufferMode(GrDepthBufferMode_t mode) { (void)mode; }
void grDepthBufferFunction(GrCmpFnc_t func) { (void)func; }
void grDepthMask(bool enabled) { (void)enabled; }
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

void grTexBind(int tex) { (void)tex; }

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
    return ((uint32_t)float_to_byte(a) << 24)
         | ((uint32_t)float_to_byte(b) << 16)
         | ((uint32_t)float_to_byte(g) << 8)
         | (uint32_t)float_to_byte(r);
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
    host.mouse_down = test_host_mouse_down;
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
            "assert glide.mouse_down('right')\n"
            "assert glide.mouse_position() == (123.0, 45.0)\n"
            "assert glide.screen_size() == (960, 720)\n"
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
            "tex = glide.upload_texture(2, 1, [128, 1, 2, 3, 255, 4, 5, 6])\n"
            "assert tex == 1\n"
            "glide.image(tex, 100.0, 120.0, 16.0, 12.0, (0.8, 0.7, 0.6, 0.5))\n"
            "glide.clear((0.25, 0.5, 0.75, 0.5), 0x1234)\n"
            "glide.set_title('binding test')\n"
            "glide.quit()\n"
            "glide.set_untextured()\n"
            "glide.set_textured_modulate()\n"
            "glide.set_blend_none()\n"
            "glide.set_blend_alpha()\n"
            "glide.set_blend_add()\n",
            "<bindings-test>", EXEC_MODE, main_mod)) {
        py_printexc();
        g_failures++;
    }

    CHECK(g_triangle_count == 5);
    CHECK(g_point_count == 1);
    CHECK(g_line_count == 1);
    CHECK(nearf(g_last_triangle[0].x, 10.0f));
    CHECK(nearf(g_last_triangle[0].y, 20.0f));
    CHECK(nearf(g_last_triangle[0].z, 0.25f));
    CHECK(nearf(g_last_triangle[0].oow, 0.5f));
    CHECK(nearf(g_last_triangle[0].u, 0.75f));
    CHECK(nearf(g_last_triangle[0].v, 0.125f));
    CHECK(nearf(g_last_triangle[2].r, 0.2f));
    CHECK(nearf(g_last_triangle[2].g, 0.3f));
    CHECK(nearf(g_last_triangle[2].b, 0.4f));
    CHECK(nearf(g_last_triangle[2].a, 1.0f));

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

    CHECK(g_last_rgb_sf == GR_BLEND_SRC_ALPHA);
    CHECK(g_last_rgb_df == GR_BLEND_ONE);
    CHECK(g_last_alpha_sf == GR_BLEND_ONE);
    CHECK(g_last_alpha_df == GR_BLEND_ONE);

    glBindingsSetHost(NULL);
    py_finalize();
    return g_failures;
}
