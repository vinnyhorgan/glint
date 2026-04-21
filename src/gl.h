#ifndef GL_H
#define GL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GLFWwindow;

/* -------------------------------------------------------------------------- */
/* Types                                                                      */
/* -------------------------------------------------------------------------- */

typedef uint32_t GrColor_t;
typedef uint32_t GrDepth_t;

typedef struct GrVertex {
    float x, y, z, oow;  /* clip-space position; oow = 1/w                 */
    float r, g, b, a;    /* vertex colour, 0..1                            */
    float u, v;          /* texture coordinates                              */
} GrVertex;

/* -------------------------------------------------------------------------- */
/* Enums                                                                      */
/* -------------------------------------------------------------------------- */

typedef enum {
    GR_BLEND_NONE = 0,
    GR_BLEND_ALPHA,
    GR_BLEND_ADD,
    GR_BLEND_MULTIPLY,
} GrAlphaBlendMode;

typedef enum {
    GR_ALPHATEST_DISABLE = 0,
    GR_ALPHATEST_GT,
    GR_ALPHATEST_EQ,
    GR_ALPHATEST_GE,
    GR_ALPHATEST_LT,
    GR_ALPHATEST_LE,
    GR_ALPHATEST_NE,
} GrAlphaTestMode;

typedef enum {
    GR_COMBINE_MODE_MODULATE = 0,
    GR_COMBINE_MODE_DECAL,
} GrCombineMode;

typedef enum {
    GR_FOG_DISABLE = 0,
    GR_FOG_LINEAR,
    GR_FOG_EXP,
    GR_FOG_EXP2,
} GrFogMode;

typedef enum {
    GR_CULL_DISABLE = 0,
    GR_CULL_NEGATIVE,
    GR_CULL_POSITIVE,
} GrCullMode;

typedef enum {
    GR_TEXFMT_RGB_565 = 0,
    GR_TEXFMT_ARGB_1555,
    GR_TEXFMT_ARGB_8888,
} GrTextureFormat;

typedef enum {
    GR_MIPMAP_DISABLE = 0,
    GR_MIPMAP_NEAREST,
    GR_MIPMAP_TRILINEAR,
} GrMipMapMode;

typedef enum {
    GR_TEXTUREFILTER_POINT_SAMPLED = 0,
    GR_TEXTUREFILTER_BILINEAR,
} GrTextureFilter;

/* -------------------------------------------------------------------------- */
/* Life-cycle                                                                 */
/* -------------------------------------------------------------------------- */

int  grInit(int win_w, int win_h);
void grShutdown(void);
void grBufferClear(GrColor_t colour, GrDepth_t depth);
void grBufferSwap(struct GLFWwindow *window);

/* -------------------------------------------------------------------------- */
/* Viewport (inside the 320x240 render target)                                */
/* -------------------------------------------------------------------------- */

void grViewport(int x, int y, int width, int height);

/* -------------------------------------------------------------------------- */
/* Render state                                                               */
/* -------------------------------------------------------------------------- */

void grAlphaBlend(GrAlphaBlendMode mode);
void grAlphaTest(GrAlphaTestMode mode, float ref);
void grColorCombine(GrCombineMode mode);
void grAlphaCombine(GrCombineMode mode);
void grFogMode(GrFogMode mode);
void grFogColorValue(GrColor_t colour);
void grFogTable(const float *table, int n);
void grCullMode(GrCullMode mode);

/* -------------------------------------------------------------------------- */
/* Textures – max 8 active, max 256x256                                       */
/* -------------------------------------------------------------------------- */

int  grTexAllocate(void);
void grTexFree(int tex);
void grTexDownloadMipMap(int tex, const void *data, int w, int h, GrTextureFormat fmt);
void grTexBind(int tex);
void grTexFilter(int tex, GrMipMapMode mm, GrTextureFilter minf, GrTextureFilter magf);

/* -------------------------------------------------------------------------- */
/* Drawing                                                                    */
/* -------------------------------------------------------------------------- */

void grDrawTriangle(const GrVertex *v0,
                    const GrVertex *v1,
                    const GrVertex *v2);
void grDrawPoint(const GrVertex *v);
void grDrawLine(const GrVertex *v0, const GrVertex *v1);

/* -------------------------------------------------------------------------- */
/* Colour helpers                                                             */
/* -------------------------------------------------------------------------- */

GrColor_t grColorPack(float r, float g, float b, float a);
void      grColorUnpack(GrColor_t c, float *r, float *g, float *b, float *a);

#ifdef __cplusplus
}
#endif

#endif /* GL_H */
