#include "gl_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures;

int run_gl_bindings_tests(void);

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

static void test_defaults(void)
{
    GrStateCore state;
    grCoreInitState(&state);
    CHECK(state.color_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(state.alpha_func == GR_COMBINE_FUNCTION_SCALE_OTHER);
    CHECK(state.rgb_sf == GR_BLEND_ONE);
    CHECK(state.rgb_df == GR_BLEND_ZERO);
    CHECK(state.alpha_test_func == GR_CMP_ALWAYS);
    CHECK(state.depth_mode == GR_DEPTHBUFFER_ZBUFFER);
    CHECK(state.depth_func == GR_CMP_LESS);
    CHECK(state.depth_mask);
    CHECK(state.clip_xmin == 0 && state.clip_ymin == 0);
    CHECK(state.clip_xmax == GR_FB_W && state.clip_ymax == GR_FB_H);
}

static void test_clip_window(void)
{
    GrStateCore state;
    grCoreInitState(&state);
    grCoreSetClipWindow(&state, -10, 5, 500, 300);
    CHECK(state.clip_xmin == 0);
    CHECK(state.clip_ymin == 5);
    CHECK(state.clip_xmax == GR_FB_W);
    CHECK(state.clip_ymax == GR_FB_H);
}

static void test_color_pack_unpack(void)
{
    GrColor4f color = {0.25f, 0.5f, 0.75f, 1.0f};
    GrColor4f unpacked;
    GrColor_t packed = grCorePackColor(&color);
    grCoreUnpackColor(packed, &unpacked);
    CHECK(nearf(unpacked.r, 64.0f / 255.0f));
    CHECK(nearf(unpacked.g, 128.0f / 255.0f));
    CHECK(nearf(unpacked.b, 191.0f / 255.0f));
    CHECK(nearf(unpacked.a, 1.0f));
}

static void test_texture_conversion(void)
{
    uint16_t rgb565[1] = {0xF81F};
    uint16_t argb1555[1] = {0xFFFF};
    uint8_t argb8888[4] = {0x80, 0x11, 0x22, 0x33};
    uint8_t out[4];

    grCoreConvertTexture(rgb565, 1, GR_TEXFMT_RGB_565, out);
    CHECK(out[0] == 255 && out[1] == 0 && out[2] == 255 && out[3] == 255);

    grCoreConvertTexture(argb1555, 1, GR_TEXFMT_ARGB_1555, out);
    CHECK(out[0] == 255 && out[1] == 255 && out[2] == 255 && out[3] == 255);

    grCoreConvertTexture(argb8888, 1, GR_TEXFMT_ARGB_8888, out);
    CHECK(out[0] == 0x11 && out[1] == 0x22 && out[2] == 0x33 && out[3] == 0x80);
}

static void test_combine(void)
{
    GrColor4f iterated = {0.25f, 0.5f, 0.75f, 0.6f};
    GrColor4f texture = {0.8f, 0.4f, 0.2f, 0.5f};
    GrColor4f constant = {1.0f, 0.5f, 0.25f, 0.75f};
    GrColor4f out = grCoreEvalColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER,
                                           GR_COMBINE_FACTOR_LOCAL,
                                           GR_COMBINE_LOCAL_ITERATED,
                                           GR_COMBINE_OTHER_TEXTURE,
                                           false,
                                           iterated,
                                           texture,
                                           constant,
                                           0.0f);
    float alpha = grCoreEvalAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER,
                                         GR_COMBINE_FACTOR_LOCAL_ALPHA,
                                         GR_COMBINE_LOCAL_ITERATED,
                                         GR_COMBINE_OTHER_TEXTURE,
                                         false,
                                         iterated,
                                         texture,
                                         constant,
                                         0.0f);
    CHECK(nearf(out.r, 0.2f));
    CHECK(nearf(out.g, 0.2f));
    CHECK(nearf(out.b, 0.15f));
    CHECK(nearf(alpha, 0.3f));
}

static void test_fog(void)
{
    GrStateCore state;
    grCoreInitState(&state);
    state.fog_mode = GR_FOG_WITH_ITERATED_ALPHA;
    CHECK(nearf(grCoreFogFactor(&state, 0.5f, 0.25f), 0.25f));

    state.fog_mode = GR_FOG_WITH_TABLE;
    memset(state.fog_table, 0, sizeof(state.fog_table));
    state.fog_table[0] = 0;
    state.fog_table[1] = 255;
    CHECK(grCoreFogFactor(&state, 1.0f / grCoreFogIndexToW(0), 0.0f) <= 0.01f);
    CHECK(grCoreFogFactor(&state, 1.0f / grCoreFogIndexToW(1), 0.0f) >= 0.99f);
}

static void test_alpha_and_depth(void)
{
    CHECK(grCoreAlphaTestPass(GR_CMP_EQUAL, 128, 128.0f / 255.0f));
    CHECK(!grCoreAlphaTestPass(GR_CMP_EQUAL, 127, 128.0f / 255.0f));
    CHECK(grCoreDepthTestPass(GR_CMP_LESS, 0.2f, 0.4f));
    CHECK(!grCoreDepthTestPass(GR_CMP_GREATER, 0.2f, 0.4f));
}

static void test_cull_and_coords(void)
{
    GrStateCore state;
    GrVertex a = {10.0f, 10.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    GrVertex b = {40.0f, 10.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    GrVertex c = {10.0f, 40.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float ndc_x;
    float ndc_y;
    grCoreInitState(&state);
    state.cull_mode = GR_CULL_POSITIVE;
    CHECK(grCoreCullTriangle(&state, &a, &b, &c));
    state.cull_mode = GR_CULL_NEGATIVE;
    CHECK(!grCoreCullTriangle(&state, &a, &b, &c));
    grCoreWindowToNdc(0.0f, 0.0f, &ndc_x, &ndc_y);
    CHECK(nearf(ndc_x, -1.0f));
    CHECK(nearf(ndc_y, 1.0f));
}

static void test_blit_rect(void)
{
    int x, y, w, h;
    grCoreComputeBlitRect(1920, 1080, &x, &y, &w, &h);
    CHECK(w == 1280);
    CHECK(h == 960);
    CHECK(x == 320);
    CHECK(y == 60);
}

int main(void)
{
    test_defaults();
    test_clip_window();
    test_color_pack_unpack();
    test_texture_conversion();
    test_combine();
    test_fog();
    test_alpha_and_depth();
    test_cull_and_coords();
    test_blit_rect();
    g_failures += run_gl_bindings_tests();

    if (g_failures != 0) {
        fprintf(stderr, "%d test failures\n", g_failures);
        return 1;
    }

    printf("gl_core tests passed\n");
    return 0;
}
