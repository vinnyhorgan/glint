#include "gl_core.h"

#include <math.h>
#include <string.h>

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float grCoreClamp01(float v)
{
    return clampf(v, 0.0f, 1.0f);
}

uint8_t grCoreFloatToByte(float v)
{
    int iv = (int)(grCoreClamp01(v) * 255.0f + 0.5f);
    if (iv < 0) iv = 0;
    if (iv > 255) iv = 255;
    return (uint8_t)iv;
}

float grCoreByteToFloat(uint8_t v)
{
    return (float)v / 255.0f;
}

void grCoreUnpackColor(GrColor_t c, GrColor4f *out)
{
    out->r = grCoreByteToFloat((uint8_t)(c & 0xFFu));
    out->g = grCoreByteToFloat((uint8_t)((c >> 8) & 0xFFu));
    out->b = grCoreByteToFloat((uint8_t)((c >> 16) & 0xFFu));
    out->a = grCoreByteToFloat((uint8_t)((c >> 24) & 0xFFu));
}

GrColor_t grCorePackColor(const GrColor4f *c)
{
    return ((uint32_t)grCoreFloatToByte(c->a) << 24)
         | ((uint32_t)grCoreFloatToByte(c->b) << 16)
         | ((uint32_t)grCoreFloatToByte(c->g) << 8)
         | (uint32_t)grCoreFloatToByte(c->r);
}

void grCoreInitState(GrStateCore *state)
{
    memset(state, 0, sizeof(*state));
    state->color_func = GR_COMBINE_FUNCTION_SCALE_OTHER;
    state->color_factor = GR_COMBINE_FACTOR_ONE;
    state->color_local = GR_COMBINE_LOCAL_ITERATED;
    state->color_other = GR_COMBINE_OTHER_ITERATED;
    state->alpha_func = GR_COMBINE_FUNCTION_SCALE_OTHER;
    state->alpha_factor = GR_COMBINE_FACTOR_ONE;
    state->alpha_local = GR_COMBINE_LOCAL_NONE;
    state->alpha_other = GR_COMBINE_OTHER_CONSTANT;
    state->rgb_sf = GR_BLEND_ONE;
    state->rgb_df = GR_BLEND_ZERO;
    state->alpha_sf = GR_BLEND_ONE;
    state->alpha_df = GR_BLEND_ZERO;
    state->alpha_test_func = GR_CMP_ALWAYS;
    state->depth_mode = GR_DEPTHBUFFER_DISABLE;
    state->depth_func = GR_CMP_LESS;
    state->depth_mask = false;
    state->shade_model = GR_SHADE_GOURAUD;
    state->constant_color = 0xFFFFFFFFu;
    grCoreSetClipWindow(state, 0, 0, GR_FB_W, GR_FB_H);
}

void grCoreSetClipWindow(GrStateCore *state, int xmin, int ymin, int xmax, int ymax)
{
    if (xmin < 0) xmin = 0;
    if (ymin < 0) ymin = 0;
    if (xmax > GR_FB_W) xmax = GR_FB_W;
    if (ymax > GR_FB_H) ymax = GR_FB_H;
    if (xmax < xmin) xmax = xmin;
    if (ymax < ymin) ymax = ymin;
    state->clip_xmin = xmin;
    state->clip_ymin = ymin;
    state->clip_xmax = xmax;
    state->clip_ymax = ymax;
}

static GrColor4f select_local(GrCombineLocal_t which,
                              GrColor4f iterated,
                              GrColor4f constant,
                              float depth)
{
    switch (which) {
        case GR_COMBINE_LOCAL_ITERATED: return iterated;
        case GR_COMBINE_LOCAL_DEPTH: {
            float v = grCoreClamp01(depth);
            return (GrColor4f){v, v, v, v};
        }
        case GR_COMBINE_LOCAL_NONE:
        case GR_COMBINE_LOCAL_CONSTANT:
        default:
            return constant;
    }
}

static GrColor4f select_other(GrCombineOther_t which,
                              GrColor4f iterated,
                              GrColor4f texture,
                              GrColor4f constant)
{
    switch (which) {
        case GR_COMBINE_OTHER_ITERATED: return iterated;
        case GR_COMBINE_OTHER_TEXTURE: return texture;
        case GR_COMBINE_OTHER_NONE:
        case GR_COMBINE_OTHER_CONSTANT:
        default:
            return constant;
    }
}

static GrColor4f factor_rgb(GrCombineFactor_t factor,
                            GrColor4f local,
                            GrColor4f other,
                            GrColor4f texture)
{
    switch (factor) {
        case GR_COMBINE_FACTOR_LOCAL:
            return (GrColor4f){local.r, local.g, local.b, 1.0f};
        case GR_COMBINE_FACTOR_OTHER_ALPHA:
            return (GrColor4f){other.a, other.a, other.a, 1.0f};
        case GR_COMBINE_FACTOR_LOCAL_ALPHA:
            return (GrColor4f){local.a, local.a, local.a, 1.0f};
        case GR_COMBINE_FACTOR_TEXTURE_ALPHA:
            return (GrColor4f){texture.a, texture.a, texture.a, 1.0f};
        case GR_COMBINE_FACTOR_ONE:
            return (GrColor4f){1.0f, 1.0f, 1.0f, 1.0f};
        case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
            return (GrColor4f){1.0f - local.r, 1.0f - local.g, 1.0f - local.b, 1.0f};
        case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
            return (GrColor4f){1.0f - other.a, 1.0f - other.a, 1.0f - other.a, 1.0f};
        case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
            return (GrColor4f){1.0f - local.a, 1.0f - local.a, 1.0f - local.a, 1.0f};
        case GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA:
            return (GrColor4f){1.0f - texture.a, 1.0f - texture.a, 1.0f - texture.a, 1.0f};
        case GR_COMBINE_FACTOR_NONE:
        case GR_COMBINE_FACTOR_ZERO:
        default:
            return (GrColor4f){0.0f, 0.0f, 0.0f, 1.0f};
    }
}

static float factor_a(GrCombineFactor_t factor,
                      GrColor4f local,
                      GrColor4f other,
                      GrColor4f texture)
{
    switch (factor) {
        case GR_COMBINE_FACTOR_LOCAL: return local.a;
        case GR_COMBINE_FACTOR_OTHER_ALPHA: return other.a;
        case GR_COMBINE_FACTOR_LOCAL_ALPHA: return local.a;
        case GR_COMBINE_FACTOR_TEXTURE_ALPHA: return texture.a;
        case GR_COMBINE_FACTOR_ONE: return 1.0f;
        case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL: return 1.0f - local.a;
        case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA: return 1.0f - other.a;
        case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA: return 1.0f - local.a;
        case GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA: return 1.0f - texture.a;
        case GR_COMBINE_FACTOR_NONE:
        case GR_COMBINE_FACTOR_ZERO:
        default:
            return 0.0f;
    }
}

static GrColor4f clamp_color(GrColor4f c)
{
    c.r = grCoreClamp01(c.r);
    c.g = grCoreClamp01(c.g);
    c.b = grCoreClamp01(c.b);
    c.a = grCoreClamp01(c.a);
    return c;
}

GrColor4f grCoreEvalColorCombine(GrCombineFunction_t func,
                                 GrCombineFactor_t factor,
                                 GrCombineLocal_t local_sel,
                                 GrCombineOther_t other_sel,
                                 bool invert,
                                 GrColor4f iterated,
                                 GrColor4f texture,
                                 GrColor4f constant,
                                 float depth)
{
    GrColor4f local = select_local(local_sel, iterated, constant, depth);
    GrColor4f other = select_other(other_sel, iterated, texture, constant);
    GrColor4f f = factor_rgb(factor, local, other, texture);
    GrColor4f out = {0.0f, 0.0f, 0.0f, 1.0f};

    switch (func) {
        case GR_COMBINE_FUNCTION_ZERO:
            break;
        case GR_COMBINE_FUNCTION_LOCAL:
            out.r = local.r; out.g = local.g; out.b = local.b;
            break;
        case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
            out.r = local.a; out.g = local.a; out.b = local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER:
            out.r = f.r * other.r; out.g = f.g * other.g; out.b = f.b * other.b;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
            out.r = f.r * other.r + local.r;
            out.g = f.g * other.g + local.g;
            out.b = f.b * other.b + local.b;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
            out.r = f.r * other.r + local.a;
            out.g = f.g * other.g + local.a;
            out.b = f.b * other.b + local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
            out.r = f.r * (other.r - local.r);
            out.g = f.g * (other.g - local.g);
            out.b = f.b * (other.b - local.b);
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
            out.r = f.r * (other.r - local.r) + local.r;
            out.g = f.g * (other.g - local.g) + local.g;
            out.b = f.b * (other.b - local.b) + local.b;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
            out.r = f.r * (other.r - local.r) + local.a;
            out.g = f.g * (other.g - local.g) + local.a;
            out.b = f.b * (other.b - local.b) + local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
            out.r = f.r * (-local.r) + local.r;
            out.g = f.g * (-local.g) + local.g;
            out.b = f.b * (-local.b) + local.b;
            break;
        case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
            out.r = f.r * (-local.r) + local.a;
            out.g = f.g * (-local.g) + local.a;
            out.b = f.b * (-local.b) + local.a;
            break;
        default:
            break;
    }

    out = clamp_color(out);
    if (invert) {
        out.r = 1.0f - out.r;
        out.g = 1.0f - out.g;
        out.b = 1.0f - out.b;
    }
    return clamp_color(out);
}

float grCoreEvalAlphaCombine(GrCombineFunction_t func,
                             GrCombineFactor_t factor,
                             GrCombineLocal_t local_sel,
                             GrCombineOther_t other_sel,
                             bool invert,
                             GrColor4f iterated,
                             GrColor4f texture,
                             GrColor4f constant,
                             float depth)
{
    GrColor4f local = select_local(local_sel, iterated, constant, depth);
    GrColor4f other = select_other(other_sel, iterated, texture, constant);
    float f = factor_a(factor, local, other, texture);
    float out = 0.0f;

    switch (func) {
        case GR_COMBINE_FUNCTION_ZERO:
            out = 0.0f;
            break;
        case GR_COMBINE_FUNCTION_LOCAL:
        case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
            out = local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER:
            out = f * other.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
        case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
            out = f * other.a + local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
            out = f * (other.a - local.a);
            break;
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
        case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
            out = f * (other.a - local.a) + local.a;
            break;
        case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
        case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
            out = f * (-local.a) + local.a;
            break;
        default:
            break;
    }

    out = grCoreClamp01(out);
    if (invert) out = 1.0f - out;
    return grCoreClamp01(out);
}

void grCoreConvertTexture(const void *src, int pixels, GrTextureFormat fmt, uint8_t *dst_rgba)
{
    int i;
    if (fmt == GR_TEXFMT_RGB_565) {
        const uint16_t *in = (const uint16_t *)src;
        for (i = 0; i < pixels; i++) {
            uint16_t p = in[i];
            int r = (p >> 11) & 0x1F;
            int g = (p >> 5) & 0x3F;
            int b = p & 0x1F;
            dst_rgba[i * 4 + 0] = (uint8_t)((r << 3) | (r >> 2));
            dst_rgba[i * 4 + 1] = (uint8_t)((g << 2) | (g >> 4));
            dst_rgba[i * 4 + 2] = (uint8_t)((b << 3) | (b >> 2));
            dst_rgba[i * 4 + 3] = 255;
        }
        return;
    }
    if (fmt == GR_TEXFMT_ARGB_1555) {
        const uint16_t *in = (const uint16_t *)src;
        for (i = 0; i < pixels; i++) {
            uint16_t p = in[i];
            int a = (p >> 15) & 0x1;
            int r = (p >> 10) & 0x1F;
            int g = (p >> 5) & 0x1F;
            int b = p & 0x1F;
            dst_rgba[i * 4 + 0] = (uint8_t)((r << 3) | (r >> 2));
            dst_rgba[i * 4 + 1] = (uint8_t)((g << 3) | (g >> 2));
            dst_rgba[i * 4 + 2] = (uint8_t)((b << 3) | (b >> 2));
            dst_rgba[i * 4 + 3] = a ? 255 : 0;
        }
        return;
    }

    {
        const uint8_t *in = (const uint8_t *)src;
        for (i = 0; i < pixels; i++) {
            dst_rgba[i * 4 + 0] = in[i * 4 + 1];
            dst_rgba[i * 4 + 1] = in[i * 4 + 2];
            dst_rgba[i * 4 + 2] = in[i * 4 + 3];
            dst_rgba[i * 4 + 3] = in[i * 4 + 0];
        }
    }
}

float grCoreFogIndexToW(int idx)
{
    return powf(2.0f, 3.0f + (float)(idx >> 2)) / (float)(8 - (idx & 3));
}

float grCoreFogFactor(const GrStateCore *state, float oow, float iterated_alpha)
{
    if (state->fog_mode == GR_FOG_DISABLE) return 0.0f;
    if (state->fog_mode == GR_FOG_WITH_ITERATED_ALPHA) return grCoreClamp01(iterated_alpha);

    if (oow <= 0.0f) return 0.0f;

    {
        float w = 1.0f / oow;
        int i;
        float prev_w = grCoreFogIndexToW(0);
        float prev_f = grCoreByteToFloat(state->fog_table[0]);

        if (w <= prev_w) return prev_f;

        for (i = 1; i < GR_FOG_TABLE_SIZE; i++) {
            float next_w = grCoreFogIndexToW(i);
            float next_f = grCoreByteToFloat(state->fog_table[i]);
            if (w <= next_w) {
                float t = (w - prev_w) / (next_w - prev_w);
                return grCoreClamp01(prev_f + (next_f - prev_f) * t);
            }
            prev_w = next_w;
            prev_f = next_f;
        }

        return prev_f;
    }
}

static bool compare_float(GrCmpFnc_t func, float lhs, float rhs)
{
    switch (func) {
        case GR_CMP_NEVER: return false;
        case GR_CMP_LESS: return lhs < rhs;
        case GR_CMP_EQUAL: return lhs == rhs;
        case GR_CMP_LEQUAL: return lhs <= rhs;
        case GR_CMP_GREATER: return lhs > rhs;
        case GR_CMP_NOTEQUAL: return lhs != rhs;
        case GR_CMP_GEQUAL: return lhs >= rhs;
        case GR_CMP_ALWAYS: default: return true;
    }
}

bool grCoreAlphaTestPass(GrCmpFnc_t func, GrAlpha_t ref, float alpha)
{
    return compare_float(func, (float)grCoreFloatToByte(alpha), (float)ref);
}

bool grCoreDepthTestPass(GrCmpFnc_t func, float src, float dst)
{
    return compare_float(func, src, dst);
}

bool grCoreCullTriangle(const GrStateCore *state,
                        const GrVertex *v0,
                        const GrVertex *v1,
                        const GrVertex *v2)
{
    float area;
    if (state->cull_mode == GR_CULL_DISABLE) return false;

    area = (v1->x - v0->x) * (v2->y - v0->y) - (v1->y - v0->y) * (v2->x - v0->x);
    if (state->cull_mode == GR_CULL_NEGATIVE) return area < 0.0f;
    return area > 0.0f;
}

void grCoreWindowToNdc(float x, float y, float *ndc_x, float *ndc_y)
{
    *ndc_x = (x / (GR_FB_W * 0.5f)) - 1.0f;
    *ndc_y = 1.0f - (y / (GR_FB_H * 0.5f));
}

void grCoreComputeBlitRect(int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int scale_x = fb_w / GR_FB_W;
    int scale_y = fb_h / GR_FB_H;
    int scale = scale_x < scale_y ? scale_x : scale_y;

    if (scale < 1) scale = 1;

    *w = GR_FB_W * scale;
    *h = GR_FB_H * scale;
    *x = (fb_w - *w) / 2;
    *y = (fb_h - *h) / 2;
}
