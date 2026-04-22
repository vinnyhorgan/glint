#include "gl.h"

#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl_core.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float x;
    float y;
    float z;
    float oow;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
} BatchVertex;

typedef struct {
    GLuint id;
    int width;
    int height;
    bool allocated;
    bool has_mipmaps;
    GrMipMapMode mipmap_mode;
    GrTextureFilter min_filter;
    GrTextureFilter mag_filter;
    GrTextureClampMode s_clamp;
    GrTextureClampMode t_clamp;
} TextureSlot;

typedef struct {
    GLuint render_prog;
    GLuint blit_prog;
    GLuint batch_vbo;
    GLuint blit_vbo;
    GLuint fbo;
    GLuint color_tex;
    GLuint depth_rb;
    GLint render_u_has_tex;
    GLint render_u_tex;
    GLint render_u_constant_color;
    GLint render_u_color_func;
    GLint render_u_color_factor;
    GLint render_u_color_local;
    GLint render_u_color_other;
    GLint render_u_color_invert;
    GLint render_u_alpha_func;
    GLint render_u_alpha_factor;
    GLint render_u_alpha_local;
    GLint render_u_alpha_other;
    GLint render_u_alpha_invert;
    GLint render_u_fog_mode;
    GLint render_u_fog_color;
    GLint render_u_fog_table;
    GLint render_u_alpha_test_func;
    GLint render_u_alpha_ref;
    GLint render_u_viewport_origin;
    GLint render_u_viewport_size;
    GLint blit_u_tex;
    TextureSlot textures[GR_MAX_TEXTURES];
    BatchVertex batch[8192];
    int batch_count;
    int bound_tex;
    int viewport_x;
    int viewport_y;
    int viewport_w;
    int viewport_h;
    int win_w;
    int win_h;
    GrStateCore core;
    bool ready;
    GrStateCore stack_core[GR_MAX_STATE_STACK];
    int stack_bound_tex[GR_MAX_STATE_STACK];
    int stack_viewport_x[GR_MAX_STATE_STACK];
    int stack_viewport_y[GR_MAX_STATE_STACK];
    int stack_viewport_w[GR_MAX_STATE_STACK];
    int stack_viewport_h[GR_MAX_STATE_STACK];
    int stack_depth;
} GlState;

static GlState g_gl;

static const char *kRenderVs[] = {
    "attribute vec4 a_pos;\n"
    "attribute vec4 a_color;\n"
    "attribute vec2 a_uv;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "varying float v_oow;\n"
    "uniform vec2 u_viewport_origin;\n"
    "uniform vec2 u_viewport_size;\n"
    "void main() {\n"
    "  float ndc_x = ((a_pos.x - u_viewport_origin.x) / u_viewport_size.x) * 2.0 - 1.0;\n"
    "  float ndc_y = 1.0 - ((a_pos.y - u_viewport_origin.y) / u_viewport_size.y) * 2.0;\n"
    "  gl_Position = vec4(ndc_x, ndc_y, a_pos.z * 2.0 - 1.0, 1.0);\n"
    "  gl_PointSize = 1.0;\n"
    "  v_color = a_color;\n"
    "  v_uv = a_uv;\n"
    "  v_oow = a_pos.w;\n"
    "}\n",
};

static const char *kRenderFs[] = {
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "varying float v_oow;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_has_tex;\n"
    "uniform vec4 u_constant_color;\n"
    "uniform int u_color_func;\n"
    "uniform int u_color_factor;\n"
    "uniform int u_color_local;\n"
    "uniform int u_color_other;\n"
    "uniform int u_color_invert;\n"
    "uniform int u_alpha_func;\n"
    "uniform int u_alpha_factor;\n"
    "uniform int u_alpha_local;\n"
    "uniform int u_alpha_other;\n"
    "uniform int u_alpha_invert;\n"
    "uniform int u_fog_mode;\n"
    "uniform vec4 u_fog_color;\n"
    "uniform float u_fog_table[64];\n"
    "uniform int u_alpha_test_func;\n"
    "uniform float u_alpha_ref;\n"
    "vec4 selectLocal(int which, float depth, vec4 iterated, vec4 constantColor) {\n"
    "  if(which == 1) return iterated;\n"
    "  if(which == 3) return vec4(depth, depth, depth, depth);\n"
    "  return constantColor;\n"
    "}\n"
    "vec4 selectOther(int which, vec4 iterated, vec4 textureColor, vec4 constantColor) {\n"
    "  if(which == 1) return iterated;\n"
    "  if(which == 2) return textureColor;\n"
    "  return constantColor;\n"
    "}\n"
    "vec3 factorRgb(int factor, vec4 localColor, vec4 otherColor, vec4 textureColor) {\n"
    "  if(factor == 2) return localColor.rgb;\n"
    "  if(factor == 3) return vec3(otherColor.a);\n"
    "  if(factor == 4) return vec3(localColor.a);\n"
    "  if(factor == 5) return vec3(textureColor.a);\n"
    "  if(factor == 6) return vec3(1.0);\n"
    "  if(factor == 7) return vec3(1.0) - localColor.rgb;\n"
    "  if(factor == 8) return vec3(1.0 - otherColor.a);\n"
    "  if(factor == 9) return vec3(1.0 - localColor.a);\n"
    "  if(factor == 10) return vec3(1.0 - textureColor.a);\n"
    "  return vec3(0.0);\n"
    "}\n",
    "float factorA(int factor, vec4 localColor, vec4 otherColor, vec4 textureColor) {\n"
    "  if(factor == 2 || factor == 4) return localColor.a;\n"
    "  if(factor == 3) return otherColor.a;\n"
    "  if(factor == 5) return textureColor.a;\n"
    "  if(factor == 6) return 1.0;\n"
    "  if(factor == 7 || factor == 9) return 1.0 - localColor.a;\n"
    "  if(factor == 8) return 1.0 - otherColor.a;\n"
    "  if(factor == 10) return 1.0 - textureColor.a;\n"
    "  return 0.0;\n"
    "}\n",
    "vec3 evalColorCombine(int func, int factor, int localSel, int otherSel, int invert, vec4 iterated, vec4 textureColor, vec4 constantColor, float depth) {\n"
    "  vec4 localColor = selectLocal(localSel, depth, iterated, constantColor);\n"
    "  vec4 otherColor = selectOther(otherSel, iterated, textureColor, constantColor);\n"
    "  vec3 f = factorRgb(factor, localColor, otherColor, textureColor);\n"
    "  vec3 outColor = vec3(0.0);\n"
    "  if(func == 1) outColor = localColor.rgb;\n"
    "  else if(func == 2) outColor = vec3(localColor.a);\n"
    "  else if(func == 3) outColor = f * otherColor.rgb;\n"
    "  else if(func == 4) outColor = f * otherColor.rgb + localColor.rgb;\n"
    "  else if(func == 5) outColor = f * otherColor.rgb + vec3(localColor.a);\n"
    "  else if(func == 6) outColor = f * (otherColor.rgb - localColor.rgb);\n"
    "  else if(func == 7) outColor = f * (otherColor.rgb - localColor.rgb) + localColor.rgb;\n"
    "  else if(func == 8) outColor = f * (otherColor.rgb - localColor.rgb) + vec3(localColor.a);\n"
    "  else if(func == 9) outColor = f * (-localColor.rgb) + localColor.rgb;\n"
    "  else if(func == 10) outColor = f * (-localColor.rgb) + vec3(localColor.a);\n"
    "  outColor = clamp(outColor, 0.0, 1.0);\n"
    "  if(invert != 0) outColor = vec3(1.0) - outColor;\n"
    "  return clamp(outColor, 0.0, 1.0);\n"
    "}\n",
    "float evalAlphaCombine(int func, int factor, int localSel, int otherSel, int invert, vec4 iterated, vec4 textureColor, vec4 constantColor, float depth) {\n"
    "  vec4 localColor = selectLocal(localSel, depth, iterated, constantColor);\n"
    "  vec4 otherColor = selectOther(otherSel, iterated, textureColor, constantColor);\n"
    "  float f = factorA(factor, localColor, otherColor, textureColor);\n"
    "  float outAlpha = 0.0;\n"
    "  if(func == 1 || func == 2) outAlpha = localColor.a;\n"
    "  else if(func == 3) outAlpha = f * otherColor.a;\n"
    "  else if(func == 4 || func == 5) outAlpha = f * otherColor.a + localColor.a;\n"
    "  else if(func == 6) outAlpha = f * (otherColor.a - localColor.a);\n"
    "  else if(func == 7 || func == 8) outAlpha = f * (otherColor.a - localColor.a) + localColor.a;\n"
    "  else if(func == 9 || func == 10) outAlpha = f * (-localColor.a) + localColor.a;\n"
    "  outAlpha = clamp(outAlpha, 0.0, 1.0);\n"
    "  if(invert != 0) outAlpha = 1.0 - outAlpha;\n"
    "  return clamp(outAlpha, 0.0, 1.0);\n"
    "}\n",
    "float fogIndexToW(int idx) {\n"
    "  return pow(2.0, 3.0 + floor(float(idx) / 4.0)) / (8.0 - mod(float(idx), 4.0));\n"
    "}\n"
    "float fogFactor(float oow, float iteratedAlpha) {\n"
    "  if(u_fog_mode == 0) return 0.0;\n"
    "  if(u_fog_mode == 1) return clamp(iteratedAlpha, 0.0, 1.0);\n"
    "  if(oow <= 0.0) return 0.0;\n"
    "  float w = 1.0 / oow;\n"
    "  float prevW = fogIndexToW(0);\n"
    "  float prevF = u_fog_table[0];\n"
    "  if(w <= prevW) return prevF;\n"
    "  for(int i = 1; i < 64; ++i) {\n"
    "    float nextW = fogIndexToW(i);\n"
    "    float nextF = u_fog_table[i];\n"
    "    if(w <= nextW) {\n"
    "      float t = (w - prevW) / (nextW - prevW);\n"
    "      return clamp(mix(prevF, nextF, t), 0.0, 1.0);\n"
    "    }\n"
    "    prevW = nextW;\n"
    "    prevF = nextF;\n"
    "  }\n"
    "  return prevF;\n"
    "}\n",
    "bool compareAlpha(int func, float lhs, float rhs) {\n"
    "  if(func == 0) return false;\n"
    "  if(func == 1) return lhs < rhs;\n"
    "  if(func == 2) return lhs == rhs;\n"
    "  if(func == 3) return lhs <= rhs;\n"
    "  if(func == 4) return lhs > rhs;\n"
    "  if(func == 5) return lhs != rhs;\n"
    "  if(func == 6) return lhs >= rhs;\n"
    "  return true;\n"
    "}\n"
    "void main() {\n"
    "  vec4 textureColor = u_has_tex != 0 ? texture2D(u_tex, v_uv) : vec4(1.0);\n"
    "  vec3 rgb = evalColorCombine(u_color_func, u_color_factor, u_color_local, u_color_other, u_color_invert, v_color, textureColor, u_constant_color, clamp(1.0 - v_oow, 0.0, 1.0));\n"
    "  float alpha = evalAlphaCombine(u_alpha_func, u_alpha_factor, u_alpha_local, u_alpha_other, u_alpha_invert, v_color, textureColor, u_constant_color, clamp(1.0 - v_oow, 0.0, 1.0));\n"
    "  float fog = fogFactor(v_oow, v_color.a);\n"
    "  rgb = mix(rgb, u_fog_color.rgb, fog);\n"
    "  float alphaByte = floor(clamp(alpha, 0.0, 1.0) * 255.0 + 0.5);\n"
    "  if(!compareAlpha(u_alpha_test_func, alphaByte, u_alpha_ref)) discard;\n"
    "  gl_FragColor = vec4(rgb, alpha);\n",
    "}\n",
};

static const char *kBlitVs[] = {
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "  v_uv = a_uv;\n",
    "}\n",
};

static const char *kBlitFs[] = {
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  vec4 c = texture2D(u_tex, v_uv);\n"
    "  gl_FragColor = vec4(c.rgb, 1.0);\n",
    "}\n",
};

static GLuint compile_shader(GLenum type, const char **src, GLsizei count)
{
    GLuint shader = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(shader, count, src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetShaderInfoLog(shader, (GLsizei)sizeof(log), &n, log);
        fprintf(stderr, "shader compile failed: %.*s\n", (int)n, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(const char **vs_src, GLsizei vs_count,
                           const char **fs_src, GLsizei fs_count,
                           bool render_program)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, vs_count);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, fs_count);
    GLuint prog;
    GLint ok = 0;

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    if (render_program) {
        glBindAttribLocation(prog, 0, "a_pos");
        glBindAttribLocation(prog, 1, "a_color");
        glBindAttribLocation(prog, 2, "a_uv");
    } else {
        glBindAttribLocation(prog, 0, "a_pos");
        glBindAttribLocation(prog, 1, "a_uv");
    }
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetProgramInfoLog(prog, (GLsizei)sizeof(log), &n, log);
        fprintf(stderr, "program link failed: %.*s\n", (int)n, log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static GLenum map_cmp_func(GrCmpFnc_t func)
{
    switch (func) {
        case GR_CMP_NEVER: return GL_NEVER;
        case GR_CMP_LESS: return GL_LESS;
        case GR_CMP_EQUAL: return GL_EQUAL;
        case GR_CMP_LEQUAL: return GL_LEQUAL;
        case GR_CMP_GREATER: return GL_GREATER;
        case GR_CMP_NOTEQUAL: return GL_NOTEQUAL;
        case GR_CMP_GEQUAL: return GL_GEQUAL;
        case GR_CMP_ALWAYS:
        default: return GL_ALWAYS;
    }
}

static GLenum map_blend_func(GrAlphaBlendFnc_t func, bool allow_alpha_saturate)
{
    switch (func) {
        case GR_BLEND_ZERO: return GL_ZERO;
        case GR_BLEND_ONE: return GL_ONE;
        case GR_BLEND_SRC_COLOR: return GL_SRC_COLOR;
        case GR_BLEND_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case GR_BLEND_DST_COLOR: return GL_DST_COLOR;
        case GR_BLEND_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case GR_BLEND_SRC_ALPHA: return GL_SRC_ALPHA;
        case GR_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case GR_BLEND_DST_ALPHA: return GL_DST_ALPHA;
        case GR_BLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case GR_BLEND_ALPHA_SATURATE: return allow_alpha_saturate ? GL_SRC_ALPHA_SATURATE : GL_ONE;
        default: return GL_ONE;
    }
}

static GLenum map_texture_filter(GrTextureFilter filter)
{
    return filter == GR_TEXTUREFILTER_BILINEAR ? GL_LINEAR : GL_NEAREST;
}

static GLenum map_wrap_mode(GrTextureClampMode clamp_mode)
{
    return clamp_mode == GR_TEXTURECLAMP_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT;
}

static int viewport_width(void)
{
    return g_gl.viewport_w > 0 ? g_gl.viewport_w : 1;
}

static int viewport_height(void)
{
    return g_gl.viewport_h > 0 ? g_gl.viewport_h : 1;
}

static void set_scissor_rect(void)
{
    int x = g_gl.core.clip_xmin;
    int y = GR_FB_H - g_gl.core.clip_ymax;
    int w = g_gl.core.clip_xmax - g_gl.core.clip_xmin;
    int h = g_gl.core.clip_ymax - g_gl.core.clip_ymin;
    glScissor(x, y, w, h);
}

static void apply_texture_params(int tex)
{
    TextureSlot *slot;
    GLenum min_filter;
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;

    slot = &g_gl.textures[tex];
    glBindTexture(GL_TEXTURE_2D, slot->id);
    if (slot->mipmap_mode == GR_MIPMAP_DISABLE || !slot->has_mipmaps) {
        min_filter = map_texture_filter(slot->min_filter);
    } else {
        min_filter = slot->min_filter == GR_TEXTUREFILTER_BILINEAR
            ? GL_LINEAR_MIPMAP_NEAREST
            : GL_NEAREST_MIPMAP_NEAREST;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)map_texture_filter(slot->mag_filter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)map_wrap_mode(slot->s_clamp));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)map_wrap_mode(slot->t_clamp));
}

static void ensure_texture_mipmaps(int tex)
{
    TextureSlot *slot;

    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;

    slot = &g_gl.textures[tex];
    if (slot->mipmap_mode == GR_MIPMAP_DISABLE || slot->width <= 0 || slot->height <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, slot->id);
    glGenerateMipmap(GL_TEXTURE_2D);
    slot->has_mipmaps = true;
}

static void apply_depth_state(void)
{
    if (g_gl.core.depth_mode == GR_DEPTHBUFFER_DISABLE) {
        glDisable(GL_DEPTH_TEST);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(map_cmp_func(g_gl.core.depth_func));
    }
    glDepthMask(g_gl.core.depth_mask ? GL_TRUE : GL_FALSE);
}

static void apply_blend_state(void)
{
    if (g_gl.core.rgb_sf == GR_BLEND_ONE && g_gl.core.rgb_df == GR_BLEND_ZERO
        && g_gl.core.alpha_sf == GR_BLEND_ONE && g_gl.core.alpha_df == GR_BLEND_ZERO) {
        glDisable(GL_BLEND);
        return;
    }

    glEnable(GL_BLEND);
    glBlendFuncSeparate(map_blend_func(g_gl.core.rgb_sf, true),
                        map_blend_func(g_gl.core.rgb_df, false),
                        map_blend_func(g_gl.core.alpha_sf, false),
                        map_blend_func(g_gl.core.alpha_df, false));
}

static void apply_uniforms(void)
{
    GrColor4f constant_color;
    GLfloat fog_table[GR_FOG_TABLE_SIZE];
    int i;

    grCoreUnpackColor(g_gl.core.constant_color, &constant_color);
    for (i = 0; i < GR_FOG_TABLE_SIZE; i++)
        fog_table[i] = grCoreByteToFloat(g_gl.core.fog_table[i]);

    glUniform1i(g_gl.render_u_has_tex, g_gl.bound_tex >= 0 ? 1 : 0);
    glUniform1i(g_gl.render_u_tex, 0);
    glUniform4f(g_gl.render_u_constant_color,
                constant_color.r, constant_color.g, constant_color.b, constant_color.a);
    glUniform1i(g_gl.render_u_color_func, g_gl.core.color_func);
    glUniform1i(g_gl.render_u_color_factor, g_gl.core.color_factor);
    glUniform1i(g_gl.render_u_color_local, g_gl.core.color_local);
    glUniform1i(g_gl.render_u_color_other, g_gl.core.color_other);
    glUniform1i(g_gl.render_u_color_invert, g_gl.core.color_invert ? 1 : 0);
    glUniform1i(g_gl.render_u_alpha_func, g_gl.core.alpha_func);
    glUniform1i(g_gl.render_u_alpha_factor, g_gl.core.alpha_factor);
    glUniform1i(g_gl.render_u_alpha_local, g_gl.core.alpha_local);
    glUniform1i(g_gl.render_u_alpha_other, g_gl.core.alpha_other);
    glUniform1i(g_gl.render_u_alpha_invert, g_gl.core.alpha_invert ? 1 : 0);
    glUniform1i(g_gl.render_u_fog_mode, g_gl.core.fog_mode);
    grCoreUnpackColor(g_gl.core.fog_color, &constant_color);
    glUniform4f(g_gl.render_u_fog_color,
                constant_color.r, constant_color.g, constant_color.b, constant_color.a);
    glUniform1fv(g_gl.render_u_fog_table, GR_FOG_TABLE_SIZE, fog_table);
    glUniform1i(g_gl.render_u_alpha_test_func, g_gl.core.alpha_test_func);
    glUniform1f(g_gl.render_u_alpha_ref, (GLfloat)g_gl.core.alpha_ref);
    glUniform2f(g_gl.render_u_viewport_origin, (GLfloat)g_gl.viewport_x, (GLfloat)g_gl.viewport_y);
    glUniform2f(g_gl.render_u_viewport_size, (GLfloat)viewport_width(), (GLfloat)viewport_height());
}

static void batch_flush(void);

static void fill_batch_vertex(BatchVertex *dst, const GrVertex *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
    dst->oow = src->oow;
    dst->r = src->r;
    dst->g = src->g;
    dst->b = src->b;
    dst->a = src->a;
    dst->u = src->u;
    dst->v = src->v;
}

static void apply_shade_model(BatchVertex *verts, int count)
{
    BatchVertex first;
    int i;

    if (count <= 1 || g_gl.core.shade_model == GR_SHADE_GOURAUD)
        return;

    first = verts[0];
    for (i = 1; i < count; ++i) {
        if ((g_gl.core.shade_model & GR_SHADE_COLOR) != 0) {
            verts[i].r = first.r;
            verts[i].g = first.g;
            verts[i].b = first.b;
        }
        if ((g_gl.core.shade_model & GR_SHADE_ALPHA) != 0)
            verts[i].a = first.a;
        if ((g_gl.core.shade_model & GR_SHADE_ST) != 0) {
            verts[i].u = first.u;
            verts[i].v = first.v;
        }
        if ((g_gl.core.shade_model & GR_SHADE_Z) != 0)
            verts[i].z = first.z;
        if ((g_gl.core.shade_model & GR_SHADE_W) != 0)
            verts[i].oow = first.oow;
    }
}

static void draw_immediate(GLenum mode, const GrVertex *verts, int count)
{
    BatchVertex batch[2];
    int i;

    if (!g_gl.ready || verts == NULL || count <= 0 || count > 2)
        return;

    batch_flush();
    for (i = 0; i < count; ++i)
        fill_batch_vertex(&batch[i], &verts[i]);
    apply_shade_model(batch, count);

    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.fbo);
    glViewport(0, 0, GR_FB_W, GR_FB_H);
    glEnable(GL_SCISSOR_TEST);
    set_scissor_rect();
    apply_depth_state();
    apply_blend_state();

    glUseProgram(g_gl.render_prog);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.batch_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sizeof(batch[0]) * (size_t)count),
                    batch);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, x));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, r));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, u));

    glActiveTexture(GL_TEXTURE0);
    if (g_gl.bound_tex >= 0)
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[g_gl.bound_tex].id);
    else
        glBindTexture(GL_TEXTURE_2D, 0);
    apply_uniforms();
    glDrawArrays(mode, 0, count);
}

static void batch_flush(void)
{
    if (!g_gl.ready || g_gl.batch_count == 0)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.fbo);
    glViewport(0, 0, GR_FB_W, GR_FB_H);
    glEnable(GL_SCISSOR_TEST);
    set_scissor_rect();
    apply_depth_state();
    apply_blend_state();

    glUseProgram(g_gl.render_prog);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.batch_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sizeof(g_gl.batch[0]) * (size_t)g_gl.batch_count),
                    g_gl.batch);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, x));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, r));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BatchVertex), (const void *)offsetof(BatchVertex, u));

    glActiveTexture(GL_TEXTURE0);
    if (g_gl.bound_tex >= 0)
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[g_gl.bound_tex].id);
    else
        glBindTexture(GL_TEXTURE_2D, 0);
    apply_uniforms();
    glDrawArrays(GL_TRIANGLES, 0, g_gl.batch_count);
    g_gl.batch_count = 0;
}

static void push_triangle(const GrVertex *v0, const GrVertex *v1, const GrVertex *v2)
{
    const GrVertex *src[3] = {v0, v1, v2};
    int i;

    if (g_gl.batch_count + 3 > (int)(sizeof(g_gl.batch) / sizeof(g_gl.batch[0])))
        batch_flush();

    for (i = 0; i < 3; i++) {
        BatchVertex *dst = &g_gl.batch[g_gl.batch_count++];
        fill_batch_vertex(dst, src[i]);
    }
}

static void push_flat_triangle(const GrVertex *v0, const GrVertex *v1, const GrVertex *v2)
{
    GrVertex a = *v0;
    GrVertex b = *v1;
    GrVertex c = *v2;

    push_triangle(&a, &b, &c);
    apply_shade_model(&g_gl.batch[g_gl.batch_count - 3], 3);
}

static void blit_to_default(int fb_w, int fb_h)
{
    GLfloat verts[24];
    int x, y, w, h;
    float l, r, t, b;

    grCoreComputeBlitRect(fb_w, fb_h, &x, &y, &w, &h);
    l = ((float)x / (float)fb_w) * 2.0f - 1.0f;
    r = ((float)(x + w) / (float)fb_w) * 2.0f - 1.0f;
    t = 1.0f - ((float)y / (float)fb_h) * 2.0f;
    b = 1.0f - ((float)(y + h) / (float)fb_h) * 2.0f;

    verts[0] = l; verts[1] = t; verts[2] = 0.0f; verts[3] = 1.0f;
    verts[4] = r; verts[5] = t; verts[6] = 1.0f; verts[7] = 1.0f;
    verts[8] = l; verts[9] = b; verts[10] = 0.0f; verts[11] = 0.0f;
    verts[12] = l; verts[13] = b; verts[14] = 0.0f; verts[15] = 0.0f;
    verts[16] = r; verts[17] = t; verts[18] = 1.0f; verts[19] = 1.0f;
    verts[20] = r; verts[21] = b; verts[22] = 1.0f; verts[23] = 0.0f;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fb_w, fb_h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_gl.blit_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gl.color_tex);
    glUniform1i(g_gl.blit_u_tex, 0);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.blit_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(GLfloat), (const void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int grInit(int win_w, int win_h)
{
    memset(&g_gl, 0, sizeof(g_gl));
    g_gl.win_w = win_w;
    g_gl.win_h = win_h;
    g_gl.bound_tex = -1;
    g_gl.viewport_w = GR_FB_W;
    g_gl.viewport_h = GR_FB_H;
    grCoreInitState(&g_gl.core);

    g_gl.render_prog = link_program(kRenderVs, (GLsizei)(sizeof(kRenderVs) / sizeof(kRenderVs[0])),
                                    kRenderFs, (GLsizei)(sizeof(kRenderFs) / sizeof(kRenderFs[0])),
                                    true);
    g_gl.blit_prog = link_program(kBlitVs, (GLsizei)(sizeof(kBlitVs) / sizeof(kBlitVs[0])),
                                  kBlitFs, (GLsizei)(sizeof(kBlitFs) / sizeof(kBlitFs[0])),
                                  false);
    if (!g_gl.render_prog || !g_gl.blit_prog) {
        grShutdown();
        return 0;
    }

    g_gl.render_u_has_tex = glGetUniformLocation(g_gl.render_prog, "u_has_tex");
    g_gl.render_u_tex = glGetUniformLocation(g_gl.render_prog, "u_tex");
    g_gl.render_u_constant_color = glGetUniformLocation(g_gl.render_prog, "u_constant_color");
    g_gl.render_u_color_func = glGetUniformLocation(g_gl.render_prog, "u_color_func");
    g_gl.render_u_color_factor = glGetUniformLocation(g_gl.render_prog, "u_color_factor");
    g_gl.render_u_color_local = glGetUniformLocation(g_gl.render_prog, "u_color_local");
    g_gl.render_u_color_other = glGetUniformLocation(g_gl.render_prog, "u_color_other");
    g_gl.render_u_color_invert = glGetUniformLocation(g_gl.render_prog, "u_color_invert");
    g_gl.render_u_alpha_func = glGetUniformLocation(g_gl.render_prog, "u_alpha_func");
    g_gl.render_u_alpha_factor = glGetUniformLocation(g_gl.render_prog, "u_alpha_factor");
    g_gl.render_u_alpha_local = glGetUniformLocation(g_gl.render_prog, "u_alpha_local");
    g_gl.render_u_alpha_other = glGetUniformLocation(g_gl.render_prog, "u_alpha_other");
    g_gl.render_u_alpha_invert = glGetUniformLocation(g_gl.render_prog, "u_alpha_invert");
    g_gl.render_u_fog_mode = glGetUniformLocation(g_gl.render_prog, "u_fog_mode");
    g_gl.render_u_fog_color = glGetUniformLocation(g_gl.render_prog, "u_fog_color");
    g_gl.render_u_fog_table = glGetUniformLocation(g_gl.render_prog, "u_fog_table");
    g_gl.render_u_alpha_test_func = glGetUniformLocation(g_gl.render_prog, "u_alpha_test_func");
    g_gl.render_u_alpha_ref = glGetUniformLocation(g_gl.render_prog, "u_alpha_ref");
    g_gl.render_u_viewport_origin = glGetUniformLocation(g_gl.render_prog, "u_viewport_origin");
    g_gl.render_u_viewport_size = glGetUniformLocation(g_gl.render_prog, "u_viewport_size");
    g_gl.blit_u_tex = glGetUniformLocation(g_gl.blit_prog, "u_tex");

    glGenBuffers(1, &g_gl.batch_vbo);
    glGenBuffers(1, &g_gl.blit_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.batch_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(g_gl.batch), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.blit_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(GLfloat) * 24), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glGenTextures(1, &g_gl.color_tex);
    glBindTexture(GL_TEXTURE_2D, g_gl.color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GR_FB_W, GR_FB_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &g_gl.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, g_gl.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, GR_FB_W, GR_FB_H);

    glGenFramebuffers(1, &g_gl.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_gl.color_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_gl.depth_rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "gl framebuffer incomplete\n");
        grShutdown();
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.fbo);
    glViewport(0, 0, GR_FB_W, GR_FB_H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    g_gl.ready = true;
    return 1;
}

void grShutdown(void)
{
    int i;
    batch_flush();
    for (i = 0; i < GR_MAX_TEXTURES; i++) {
        if (g_gl.textures[i].allocated)
            glDeleteTextures(1, &g_gl.textures[i].id);
    }
    if (g_gl.batch_vbo) glDeleteBuffers(1, &g_gl.batch_vbo);
    if (g_gl.blit_vbo) glDeleteBuffers(1, &g_gl.blit_vbo);
    if (g_gl.color_tex) glDeleteTextures(1, &g_gl.color_tex);
    if (g_gl.depth_rb) glDeleteRenderbuffers(1, &g_gl.depth_rb);
    if (g_gl.fbo) glDeleteFramebuffers(1, &g_gl.fbo);
    if (g_gl.render_prog) glDeleteProgram(g_gl.render_prog);
    if (g_gl.blit_prog) glDeleteProgram(g_gl.blit_prog);
    memset(&g_gl, 0, sizeof(g_gl));
}

void grBufferClear(GrColor_t color, GrAlpha_t alpha, GrDepth_t depth)
{
    GrColor4f c;
    GLboolean restore_depth_mask;

    batch_flush();
    grCoreUnpackColor(color, &c);
    c.a = grCoreByteToFloat(alpha);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.fbo);
    glViewport(0, 0, GR_FB_W, GR_FB_H);
    glEnable(GL_SCISSOR_TEST);
    set_scissor_rect();
    glClearColor(c.r, c.g, c.b, c.a);
    glClearDepthf((GLfloat)depth / 65535.0f);
    restore_depth_mask = g_gl.core.depth_mask ? GL_TRUE : GL_FALSE;
    if (!g_gl.core.depth_mask)
        glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(restore_depth_mask);
}

void grBufferSwap(struct GLFWwindow *window)
{
    int fb_w = g_gl.win_w;
    int fb_h = g_gl.win_h;
    batch_flush();
    if (window)
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
    g_gl.win_w = fb_w;
    g_gl.win_h = fb_h;
    blit_to_default(fb_w, fb_h);
    if (window)
        glfwSwapBuffers(window);
}

void grBufferSwapCurrent(void)
{
    GLFWwindow *window = glfwGetCurrentContext();
    grBufferSwap(window);
}

void grViewport(int x, int y, int width, int height)
{
    batch_flush();
    g_gl.viewport_x = x;
    g_gl.viewport_y = y;
    g_gl.viewport_w = width > 0 ? width : 1;
    g_gl.viewport_h = height > 0 ? height : 1;
}

void grClipWindow(int xmin, int ymin, int xmax, int ymax)
{
    batch_flush();
    grCoreSetClipWindow(&g_gl.core, xmin, ymin, xmax, ymax);
}

void grColorCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert)
{
    batch_flush();
    g_gl.core.color_func = func;
    g_gl.core.color_factor = factor;
    g_gl.core.color_local = local;
    g_gl.core.color_other = other;
    g_gl.core.color_invert = invert;
}

void grAlphaCombine(GrCombineFunction_t func,
                    GrCombineFactor_t factor,
                    GrCombineLocal_t local,
                    GrCombineOther_t other,
                    bool invert)
{
    batch_flush();
    g_gl.core.alpha_func = func;
    g_gl.core.alpha_factor = factor;
    g_gl.core.alpha_local = local;
    g_gl.core.alpha_other = other;
    g_gl.core.alpha_invert = invert;
}

void grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf,
                          GrAlphaBlendFnc_t rgb_df,
                          GrAlphaBlendFnc_t alpha_sf,
                          GrAlphaBlendFnc_t alpha_df)
{
    batch_flush();
    g_gl.core.rgb_sf = rgb_sf;
    g_gl.core.rgb_df = rgb_df;
    g_gl.core.alpha_sf = alpha_sf;
    g_gl.core.alpha_df = alpha_df;
}

void grAlphaTestFunction(GrCmpFnc_t func)
{
    batch_flush();
    g_gl.core.alpha_test_func = func;
}

void grAlphaTestReferenceValue(GrAlpha_t value)
{
    batch_flush();
    g_gl.core.alpha_ref = value;
}

void grConstantColorValue(GrColor_t color)
{
    batch_flush();
    g_gl.core.constant_color = color;
}

void grDepthBufferMode(GrDepthBufferMode_t mode)
{
    batch_flush();
    g_gl.core.depth_mode = mode;
}

void grDepthBufferFunction(GrCmpFnc_t func)
{
    batch_flush();
    g_gl.core.depth_func = func;
}

void grDepthMask(bool enabled)
{
    batch_flush();
    g_gl.core.depth_mask = enabled;
}

void grCullMode(GrCullMode_t mode)
{
    batch_flush();
    g_gl.core.cull_mode = mode;
}

void grShadeModel(GrShadeModel_t mode)
{
    batch_flush();
    g_gl.core.shade_model = mode;
}

void grFogMode(GrFogMode_t mode)
{
    batch_flush();
    g_gl.core.fog_mode = mode;
}

void grFogColorValue(GrColor_t color)
{
    batch_flush();
    g_gl.core.fog_color = color;
}

void grFogTable(const GrFog_t table[GR_FOG_TABLE_SIZE])
{
    batch_flush();
    memcpy(g_gl.core.fog_table, table, sizeof(g_gl.core.fog_table));
}

void grPushState(void)
{
    int d = g_gl.stack_depth;
    if (d >= GR_MAX_STATE_STACK)
        return;
    g_gl.stack_core[d] = g_gl.core;
    g_gl.stack_bound_tex[d] = g_gl.bound_tex;
    g_gl.stack_viewport_x[d] = g_gl.viewport_x;
    g_gl.stack_viewport_y[d] = g_gl.viewport_y;
    g_gl.stack_viewport_w[d] = g_gl.viewport_w;
    g_gl.stack_viewport_h[d] = g_gl.viewport_h;
    g_gl.stack_depth = d + 1;
}

void grPopState(void)
{
    int d = g_gl.stack_depth;
    if (d <= 0)
        return;
    d--;
    batch_flush();
    g_gl.core = g_gl.stack_core[d];
    g_gl.bound_tex = g_gl.stack_bound_tex[d];
    g_gl.viewport_x = g_gl.stack_viewport_x[d];
    g_gl.viewport_y = g_gl.stack_viewport_y[d];
    g_gl.viewport_w = g_gl.stack_viewport_w[d];
    g_gl.viewport_h = g_gl.stack_viewport_h[d];
    g_gl.stack_depth = d;
}

int grTexAllocate(void)
{
    int i;
    for (i = 0; i < GR_MAX_TEXTURES; i++) {
        TextureSlot *slot = &g_gl.textures[i];
        if (!slot->allocated) {
            memset(slot, 0, sizeof(*slot));
            glGenTextures(1, &slot->id);
            slot->allocated = true;
            slot->mipmap_mode = GR_MIPMAP_DISABLE;
            slot->min_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
            slot->mag_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
            slot->s_clamp = GR_TEXTURECLAMP_WRAP;
            slot->t_clamp = GR_TEXTURECLAMP_WRAP;
            apply_texture_params(i);
            return i;
        }
    }
    return -1;
}

void grTexFree(int tex)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;
    batch_flush();
    glDeleteTextures(1, &g_gl.textures[tex].id);
    memset(&g_gl.textures[tex], 0, sizeof(g_gl.textures[tex]));
    if (g_gl.bound_tex == tex)
        g_gl.bound_tex = -1;
}

void grTexDownloadMipMap(int tex, const void *data, int w, int h, GrTextureFormat fmt)
{
    TextureSlot *slot;
    size_t size;
    uint8_t *rgba;

    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;
    if (!data || w <= 0 || h <= 0 || w > GR_MAX_TEX_SIZE || h > GR_MAX_TEX_SIZE)
        return;

    slot = &g_gl.textures[tex];
    size = (size_t)w * (size_t)h * 4u;
    rgba = (uint8_t *)malloc(size);
    if (!rgba)
        return;

    grCoreConvertTexture(data, w * h, fmt, rgba);
    batch_flush();
    glBindTexture(GL_TEXTURE_2D, slot->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    slot->width = w;
    slot->height = h;
    slot->has_mipmaps = false;
    ensure_texture_mipmaps(tex);
    apply_texture_params(tex);
    free(rgba);
}

void grTexBind(int tex)
{
    batch_flush();
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        g_gl.bound_tex = -1;
    else
        g_gl.bound_tex = tex;
}

void grTexFilter(int tex, GrMipMapMode mm, GrTextureFilter minf, GrTextureFilter magf)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;
    batch_flush();
    g_gl.textures[tex].mipmap_mode = mm;
    g_gl.textures[tex].min_filter = minf;
    g_gl.textures[tex].mag_filter = magf;
    ensure_texture_mipmaps(tex);
    apply_texture_params(tex);
}

void grTexClampMode(int tex, GrTextureClampMode s_clamp, GrTextureClampMode t_clamp)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g_gl.textures[tex].allocated)
        return;
    batch_flush();
    g_gl.textures[tex].s_clamp = s_clamp;
    g_gl.textures[tex].t_clamp = t_clamp;
    apply_texture_params(tex);
}

void grDrawTriangle(const GrVertex *v0, const GrVertex *v1, const GrVertex *v2)
{
    if (!v0 || !v1 || !v2)
        return;
    if (grCoreCullTriangle(&g_gl.core, v0, v1, v2))
        return;
    push_flat_triangle(v0, v1, v2);
}

void grDrawPoint(const GrVertex *v)
{
    if (!v)
        return;
    draw_immediate(GL_POINTS, v, 1);
}

void grDrawLine(const GrVertex *v0, const GrVertex *v1)
{
    GrVertex line[2];

    if (!v0 || !v1)
        return;

    line[0] = *v0;
    line[1] = *v1;
    draw_immediate(GL_LINES, line, 2);
}

GrColor_t grColorPack(float r, float g, float b, float a)
{
    GrColor4f c = {r, g, b, a};
    return grCorePackColor(&c);
}

void grColorUnpack(GrColor_t c, float *r, float *g, float *b, float *a)
{
    GrColor4f out;
    grCoreUnpackColor(c, &out);
    if (r) *r = out.r;
    if (g) *g = out.g;
    if (b) *b = out.b;
    if (a) *a = out.a;
}
