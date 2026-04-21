#ifndef GL_H
#define GL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GLFWwindow;

#define GR_FB_W 320
#define GR_FB_H 240
#define GR_MAX_TEXTURES 8
#define GR_MAX_TEX_SIZE 256
#define GR_FOG_TABLE_SIZE 64

typedef uint8_t GrAlpha_t;
typedef uint8_t GrFog_t;
typedef uint16_t GrDepth_t;
typedef uint32_t GrColor_t;

typedef struct GrVertex {
    float x, y;
    float z;
    float oow;
    float r, g, b, a;
    float u, v;
} GrVertex;

typedef enum {
    GR_CMP_NEVER = 0,
    GR_CMP_LESS,
    GR_CMP_EQUAL,
    GR_CMP_LEQUAL,
    GR_CMP_GREATER,
    GR_CMP_NOTEQUAL,
    GR_CMP_GEQUAL,
    GR_CMP_ALWAYS,
} GrCmpFnc_t;

typedef enum {
    GR_BLEND_ZERO = 0,
    GR_BLEND_ONE,
    GR_BLEND_SRC_COLOR,
    GR_BLEND_ONE_MINUS_SRC_COLOR,
    GR_BLEND_DST_COLOR,
    GR_BLEND_ONE_MINUS_DST_COLOR,
    GR_BLEND_SRC_ALPHA,
    GR_BLEND_ONE_MINUS_SRC_ALPHA,
    GR_BLEND_DST_ALPHA,
    GR_BLEND_ONE_MINUS_DST_ALPHA,
    GR_BLEND_ALPHA_SATURATE,
} GrAlphaBlendFnc_t;

typedef enum {
    GR_COMBINE_FUNCTION_ZERO = 0,
    GR_COMBINE_FUNCTION_LOCAL,
    GR_COMBINE_FUNCTION_LOCAL_ALPHA,
    GR_COMBINE_FUNCTION_SCALE_OTHER,
    GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL,
    GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA,
    GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL,
    GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL,
    GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA,
    GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL,
    GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA,
    GR_COMBINE_FUNCTION_BLEND_OTHER = GR_COMBINE_FUNCTION_SCALE_OTHER,
    GR_COMBINE_FUNCTION_BLEND = GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL,
    GR_COMBINE_FUNCTION_BLEND_LOCAL = GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL,
} GrCombineFunction_t;

typedef enum {
    GR_COMBINE_FACTOR_NONE = 0,
    GR_COMBINE_FACTOR_ZERO,
    GR_COMBINE_FACTOR_LOCAL,
    GR_COMBINE_FACTOR_OTHER_ALPHA,
    GR_COMBINE_FACTOR_LOCAL_ALPHA,
    GR_COMBINE_FACTOR_TEXTURE_ALPHA,
    GR_COMBINE_FACTOR_ONE,
    GR_COMBINE_FACTOR_ONE_MINUS_LOCAL,
    GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA,
    GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA,
    GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA,
} GrCombineFactor_t;

typedef enum {
    GR_COMBINE_LOCAL_NONE = 0,
    GR_COMBINE_LOCAL_ITERATED,
    GR_COMBINE_LOCAL_CONSTANT,
    GR_COMBINE_LOCAL_DEPTH,
} GrCombineLocal_t;

typedef enum {
    GR_COMBINE_OTHER_NONE = 0,
    GR_COMBINE_OTHER_ITERATED,
    GR_COMBINE_OTHER_TEXTURE,
    GR_COMBINE_OTHER_CONSTANT,
} GrCombineOther_t;

typedef enum {
    GR_FOG_DISABLE = 0,
    GR_FOG_WITH_ITERATED_ALPHA,
    GR_FOG_WITH_TABLE,
} GrFogMode_t;

typedef enum {
    GR_CULL_DISABLE = 0,
    GR_CULL_NEGATIVE,
    GR_CULL_POSITIVE,
} GrCullMode_t;

typedef enum {
    GR_DEPTHBUFFER_DISABLE = 0,
    GR_DEPTHBUFFER_ZBUFFER,
} GrDepthBufferMode_t;

typedef enum {
    GR_SHADE_GOURAUD = 0,
    GR_SHADE_COLOR = 1 << 0,
    GR_SHADE_ALPHA = 1 << 1,
    GR_SHADE_ST = 1 << 2,
    GR_SHADE_Z = 1 << 3,
    GR_SHADE_W = 1 << 4,
    GR_SHADE_FLAT = GR_SHADE_COLOR | GR_SHADE_ALPHA | GR_SHADE_ST | GR_SHADE_Z | GR_SHADE_W,
} GrShadeModel_t;

typedef enum {
    GR_TEXFMT_RGB_565 = 0,
    GR_TEXFMT_ARGB_1555,
    GR_TEXFMT_ARGB_8888,
} GrTextureFormat;

typedef enum {
    GR_MIPMAP_DISABLE = 0,
    GR_MIPMAP_NEAREST,
    GR_MIPMAP_NEAREST_DITHER,
} GrMipMapMode;

typedef enum {
    GR_TEXTUREFILTER_POINT_SAMPLED = 0,
    GR_TEXTUREFILTER_BILINEAR,
} GrTextureFilter;

typedef enum {
    GR_TEXTURECLAMP_WRAP = 0,
    GR_TEXTURECLAMP_CLAMP,
} GrTextureClampMode;

int  grInit(int win_w, int win_h);
void grShutdown(void);
void grBufferClear(GrColor_t color, GrAlpha_t alpha, GrDepth_t depth);
void grBufferSwap(struct GLFWwindow *window);
void grBufferSwapCurrent(void);

void grViewport(int x, int y, int width, int height);
void grClipWindow(int xmin, int ymin, int xmax, int ymax);

void grColorCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert);
void grAlphaCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert);
void grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf,
                          GrAlphaBlendFnc_t rgb_df,
                          GrAlphaBlendFnc_t alpha_sf,
                          GrAlphaBlendFnc_t alpha_df);
void grAlphaTestFunction(GrCmpFnc_t func);
void grAlphaTestReferenceValue(GrAlpha_t value);
void grConstantColorValue(GrColor_t color);
void grDepthBufferMode(GrDepthBufferMode_t mode);
void grDepthBufferFunction(GrCmpFnc_t func);
void grDepthMask(bool enabled);
void grCullMode(GrCullMode_t mode);
void grShadeModel(GrShadeModel_t mode);
void grFogMode(GrFogMode_t mode);
void grFogColorValue(GrColor_t color);
void grFogTable(const GrFog_t table[GR_FOG_TABLE_SIZE]);

int  grTexAllocate(void);
void grTexFree(int tex);
void grTexDownloadMipMap(int tex, const void *data, int w, int h, GrTextureFormat fmt);
void grTexBind(int tex);
void grTexFilter(int tex, GrMipMapMode mm, GrTextureFilter minf, GrTextureFilter magf);
void grTexClampMode(int tex, GrTextureClampMode s_clamp, GrTextureClampMode t_clamp);

void grDrawTriangle(const GrVertex *v0,
                    const GrVertex *v1,
                    const GrVertex *v2);
void grDrawPoint(const GrVertex *v);
void grDrawLine(const GrVertex *v0, const GrVertex *v1);

GrColor_t grColorPack(float r, float g, float b, float a);
void      grColorUnpack(GrColor_t c, float *r, float *g, float *b, float *a);

#ifdef __cplusplus
}
#endif

#endif
