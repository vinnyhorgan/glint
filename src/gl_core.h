#ifndef GL_CORE_H
#define GL_CORE_H

#include "gl.h"

typedef struct {
    float r;
    float g;
    float b;
    float a;
} GrColor4f;

typedef struct {
    GrCombineFunction_t color_func;
    GrCombineFactor_t color_factor;
    GrCombineLocal_t color_local;
    GrCombineOther_t color_other;
    bool color_invert;

    GrCombineFunction_t alpha_func;
    GrCombineFactor_t alpha_factor;
    GrCombineLocal_t alpha_local;
    GrCombineOther_t alpha_other;
    bool alpha_invert;

    GrAlphaBlendFnc_t rgb_sf;
    GrAlphaBlendFnc_t rgb_df;
    GrAlphaBlendFnc_t alpha_sf;
    GrAlphaBlendFnc_t alpha_df;

    GrCmpFnc_t alpha_test_func;
    GrAlpha_t alpha_ref;

    GrDepthBufferMode_t depth_mode;
    GrCmpFnc_t depth_func;
    bool depth_mask;

    GrCullMode_t cull_mode;
    GrShadeModel_t shade_model;

    GrFogMode_t fog_mode;
    GrColor_t fog_color;
    GrFog_t fog_table[GR_FOG_TABLE_SIZE];

    GrColor_t constant_color;

    int clip_xmin;
    int clip_ymin;
    int clip_xmax;
    int clip_ymax;
} GrStateCore;

void grCoreInitState(GrStateCore *state);
void grCoreSetClipWindow(GrStateCore *state, int xmin, int ymin, int xmax, int ymax);

float grCoreClamp01(float v);
uint8_t grCoreFloatToByte(float v);
float grCoreByteToFloat(uint8_t v);

void grCoreUnpackColor(GrColor_t c, GrColor4f *out);
GrColor_t grCorePackColor(const GrColor4f *c);

void grCoreConvertTexture(const void *src, int pixels, GrTextureFormat fmt, uint8_t *dst_rgba);
float grCoreFogIndexToW(int idx);
float grCoreFogFactor(const GrStateCore *state, float oow, float iterated_alpha);
bool grCoreAlphaTestPass(GrCmpFnc_t func, GrAlpha_t ref, float alpha);
bool grCoreDepthTestPass(GrCmpFnc_t func, float src, float dst);
bool grCoreCullTriangle(const GrStateCore *state,
                        const GrVertex *v0,
                        const GrVertex *v1,
                        const GrVertex *v2);
void grCoreWindowToNdc(float x, float y, float *ndc_x, float *ndc_y);
void grCoreComputeBlitRect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);

GrColor4f grCoreEvalColorCombine(GrCombineFunction_t func,
                                 GrCombineFactor_t factor,
                                 GrCombineLocal_t local,
                                 GrCombineOther_t other,
                                 bool invert,
                                 GrColor4f iterated,
                                 GrColor4f texture,
                                 GrColor4f constant,
                                 float depth);
float grCoreEvalAlphaCombine(GrCombineFunction_t func,
                             GrCombineFactor_t factor,
                             GrCombineLocal_t local,
                             GrCombineOther_t other,
                             bool invert,
                             GrColor4f iterated,
                             GrColor4f texture,
                             GrColor4f constant,
                             float depth);

#endif
