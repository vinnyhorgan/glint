#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gles2.h>

#include "gl.h"

#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */

#define GR_FB_W          320
#define GR_FB_H          240
#define GR_MAX_TEXTURES  8
#define GR_MAX_TEX_SIZE  256
#define GR_BATCH_VERTS   12288   /* 4096 triangles, 40 bytes/vert  */
#define GR_VBO_COUNT     3       /* triple-buffered                */

/* -------------------------------------------------------------------------- */
/* Internal types                                                             */
/* -------------------------------------------------------------------------- */

typedef struct {
    GLuint id;
    int    used;
    int    w, h;
} GrTexSlot;

typedef struct {
    float *data;     /* interleaved: x,y,z,w  r,g,b,a  u,v  (10 floats) */
    int    count;
    int    max;
} GrBatch;

typedef struct {
    /* FBO */
    GLuint fbo;
    GLuint colour_tex;
    GLuint depth_rbo;

    /* blit */
    GLuint blit_prog;
    GLuint blit_vbo;
    GLint  blit_a_pos;
    GLint  blit_u_tex;

    /* batch shader */
    GLuint batch_prog;
    GLint  a_pos;
    GLint  a_col;
    GLint  a_uv;
    GLint  u_tex;
    GLint  u_col_combine;
    GLint  u_alpha_combine;
    GLint  u_alpha_test;
    GLint  u_alpha_ref;
    GLint  u_fog_mode;
    GLint  u_fog_col;
    GLint  u_fog_start;
    GLint  u_fog_end;
    GLint  u_has_tex;

    /* VBO ring */
    GLuint vbos[GR_VBO_COUNT];
    int    vbo_idx;

    /* viewport inside the 320x240 target */
    int vp_x, vp_y, vp_w, vp_h;

    /* batch */
    GrBatch batch;

    /* state */
    struct {
        GrAlphaBlendMode alpha_blend;
        GrAlphaTestMode  alpha_test;
        float            alpha_ref;
        GrCombineMode    col_combine;
        GrCombineMode    alpha_combine;
        GrFogMode        fog_mode;
        GrColor_t        fog_col;
        float            fog_start;
        float            fog_end;
        GrCullMode       cull;
        int              bound_tex;
    } state;

    /* textures */
    GrTexSlot textures[GR_MAX_TEXTURES];

    /* window size (for blit) */
    int win_w, win_h;
} GrContext;

/* -------------------------------------------------------------------------- */
/* Globals                                                                    */
/* -------------------------------------------------------------------------- */

static GrContext g;

/* -------------------------------------------------------------------------- */
/* Shaders                                                                    */
/* -------------------------------------------------------------------------- */

static const char s_vbatch[] =
    "attribute vec4 a_pos;\n"
    "attribute vec4 a_col;\n"
    "attribute vec2 a_uv;\n"
    "varying vec4 v_col;\n"
    "varying vec2 v_uv;\n"
    "varying float v_fog_z;\n"
    "void main(){\n"
    "  float w=1.0/a_pos.w;\n"
    "  gl_Position=vec4(a_pos.x,a_pos.y,a_pos.z,w);\n"
    "  v_col=a_col;\n"
    "  v_uv=a_uv;\n"
    "  v_fog_z=w;\n"
    "}\n";

static const char s_fbatch[] =
    "precision mediump float;\n"
    "varying vec4 v_col;\n"
    "varying vec2 v_uv;\n"
    "varying float v_fog_z;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_col_combine;\n"
    "uniform int u_alpha_combine;\n"
    "uniform int u_alpha_test;\n"
    "uniform float u_alpha_ref;\n"
    "uniform int u_fog_mode;\n"
    "uniform vec4 u_fog_col;\n"
    "uniform float u_fog_start;\n"
    "uniform float u_fog_end;\n"
    "uniform int u_has_tex;\n"
    "void main(){\n"
    "  vec4 tex=vec4(1.0);\n"
    "  if(u_has_tex!=0) tex=texture2D(u_tex,v_uv);\n"
    "  vec4 c;\n"
    "  if(u_col_combine==0) c.rgb=v_col.rgb*tex.rgb;\n"
    "  else c.rgb=tex.rgb;\n"
    "  if(u_alpha_combine==0) c.a=v_col.a*tex.a;\n"
    "  else c.a=tex.a;\n"
    "  if(u_alpha_test!=0){\n"
    "    bool pass=true;\n"
    "    if(u_alpha_test==1) pass=c.a>u_alpha_ref;\n"
    "    else if(u_alpha_test==2) pass=c.a==u_alpha_ref;\n"
    "    else if(u_alpha_test==3) pass=c.a>=u_alpha_ref;\n"
    "    else if(u_alpha_test==4) pass=c.a<u_alpha_ref;\n"
    "    else if(u_alpha_test==5) pass=c.a<=u_alpha_ref;\n"
    "    else if(u_alpha_test==6) pass=c.a!=u_alpha_ref;\n"
    "    if(!pass) discard;\n"
    "  }\n"
    "  if(u_fog_mode!=0){\n"
    "    float z=max(v_fog_z,0.0001);\n"
    "    float fog;\n"
    "    if(u_fog_mode==1) fog=clamp((u_fog_end-z)/(u_fog_end-u_fog_start),0.0,1.0);\n"
    "    else if(u_fog_mode==2) fog=exp(-u_fog_start*z);\n"
    "    else fog=exp(-(u_fog_start*z)*(u_fog_start*z));\n"
    "    c.rgb=mix(u_fog_col.rgb,c.rgb,fog);\n"
    "  }\n"
    "  gl_FragColor=c;\n"
    "}\n";

static const char s_vblit[] =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_uv;\n"
    "void main(){\n"
    "  gl_Position=vec4(a_pos,0.0,1.0);\n"
    "  v_uv=a_pos*0.5+0.5;\n"
    "}\n";

static const char s_fblit[] =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){\n"
    "  gl_FragColor=vec4(texture2D(u_tex,v_uv).rgb,1.0);\n"
    "}\n";

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint ok = GL_FALSE;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    GLint ok = GL_FALSE;
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static void apply_blend(GrAlphaBlendMode mode)
{
    switch (mode) {
        case GR_BLEND_NONE:
            glDisable(GL_BLEND);
            break;
        case GR_BLEND_ALPHA:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case GR_BLEND_ADD:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case GR_BLEND_MULTIPLY:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
    }
}

static void apply_uniforms(void)
{
    glUniform1i(g.u_col_combine,    g.state.col_combine);
    glUniform1i(g.u_alpha_combine,  g.state.alpha_combine);
    glUniform1i(g.u_alpha_test,     g.state.alpha_test);
    glUniform1f(g.u_alpha_ref,      g.state.alpha_ref);
    glUniform1i(g.u_fog_mode,       g.state.fog_mode);
    {
        float fr, fg, fb, fa;
        grColorUnpack(g.state.fog_col, &fr, &fg, &fb, &fa);
        glUniform4f(g.u_fog_col, fr, fg, fb, fa);
    }
    glUniform1f(g.u_fog_start,      g.state.fog_start);
    glUniform1f(g.u_fog_end,        g.state.fog_end);
    glUniform1i(g.u_has_tex,        g.state.bound_tex >= 0 ? 1 : 0);

    if (g.state.bound_tex >= 0 && g.textures[g.state.bound_tex].used) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g.textures[g.state.bound_tex].id);
        glUniform1i(g.u_tex, 0);
    }
}

/* -------------------------------------------------------------------------- */
/* Batch                                                                      */
/* -------------------------------------------------------------------------- */

static void batch_flush(void)
{
    if (g.batch.count == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, g.fbo);
    glViewport(g.vp_x, g.vp_y, g.vp_w, g.vp_h);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    apply_blend(g.state.alpha_blend);

    glUseProgram(g.batch_prog);
    apply_uniforms();

    glBindBuffer(GL_ARRAY_BUFFER, g.vbos[g.vbo_idx]);
    glBufferData(GL_ARRAY_BUFFER,
                 g.batch.count * 10 * sizeof(float),
                 g.batch.data,
                 GL_STREAM_DRAW);

    glEnableVertexAttribArray((GLuint)g.a_pos);
    glEnableVertexAttribArray((GLuint)g.a_col);
    glEnableVertexAttribArray((GLuint)g.a_uv);

    glVertexAttribPointer((GLuint)g.a_pos, 4, GL_FLOAT, GL_FALSE, 40,
                          (const void *)(0));
    glVertexAttribPointer((GLuint)g.a_col, 4, GL_FLOAT, GL_FALSE, 40,
                          (const void *)(16));
    glVertexAttribPointer((GLuint)g.a_uv,  2, GL_FLOAT, GL_FALSE, 40,
                          (const void *)(32));

    glDrawArrays(GL_TRIANGLES, 0, g.batch.count);

    glDisableVertexAttribArray((GLuint)g.a_pos);
    glDisableVertexAttribArray((GLuint)g.a_col);
    glDisableVertexAttribArray((GLuint)g.a_uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g.batch.count = 0;
    g.vbo_idx = (g.vbo_idx + 1) % GR_VBO_COUNT;
}

static void batch_push(const GrVertex *v)
{
    if (g.batch.count >= g.batch.max) batch_flush();

    float *p = g.batch.data + g.batch.count * 10;
    p[0] = v->x;   p[1] = v->y;   p[2] = v->z;   p[3] = v->oow;
    p[4] = v->r;   p[5] = v->g;   p[6] = v->b;   p[7] = v->a;
    p[8] = v->u;   p[9] = v->v;
    g.batch.count++;
}

/* -------------------------------------------------------------------------- */
/* Culling                                                                    */
/* -------------------------------------------------------------------------- */

static int cull_tri(const GrVertex *v0,
                    const GrVertex *v1,
                    const GrVertex *v2)
{
    if (g.state.cull == GR_CULL_DISABLE) return 0;

    float x0 = v0->x * v0->oow;
    float y0 = v0->y * v0->oow;
    float x1 = v1->x * v1->oow;
    float y1 = v1->y * v1->oow;
    float x2 = v2->x * v2->oow;
    float y2 = v2->y * v2->oow;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);

    if (g.state.cull == GR_CULL_NEGATIVE && area < 0.0f) return 1;
    if (g.state.cull == GR_CULL_POSITIVE && area > 0.0f) return 1;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

int grInit(int win_w, int win_h)
{
    memset(&g, 0, sizeof(g));
    g.win_w = win_w;
    g.win_h = win_h;

    /* --- batch buffer --- */
    g.batch.max   = GR_BATCH_VERTS;
    g.batch.count = 0;
    g.batch.data  = (float *)malloc(GR_BATCH_VERTS * 10 * sizeof(float));
    if (!g.batch.data) return 0;

    /* --- FBO --- */
    glGenTextures(1, &g.colour_tex);
    glBindTexture(GL_TEXTURE_2D, g.colour_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GR_FB_W, GR_FB_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &g.depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g.depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                          GR_FB_W, GR_FB_H);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &g.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g.colour_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, g.depth_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* --- batch VBOs --- */
    glGenBuffers(GR_VBO_COUNT, g.vbos);

    /* --- batch shader --- */
    GLuint vs = compile_shader(GL_VERTEX_SHADER, s_vbatch);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_fbatch);
    if (!vs || !fs) return 0;
    g.batch_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!g.batch_prog) return 0;

    g.a_pos           = glGetAttribLocation(g.batch_prog, "a_pos");
    g.a_col           = glGetAttribLocation(g.batch_prog, "a_col");
    g.a_uv            = glGetAttribLocation(g.batch_prog, "a_uv");
    g.u_tex           = glGetUniformLocation(g.batch_prog, "u_tex");
    g.u_col_combine   = glGetUniformLocation(g.batch_prog, "u_col_combine");
    g.u_alpha_combine = glGetUniformLocation(g.batch_prog, "u_alpha_combine");
    g.u_alpha_test    = glGetUniformLocation(g.batch_prog, "u_alpha_test");
    g.u_alpha_ref     = glGetUniformLocation(g.batch_prog, "u_alpha_ref");
    g.u_fog_mode      = glGetUniformLocation(g.batch_prog, "u_fog_mode");
    g.u_fog_col       = glGetUniformLocation(g.batch_prog, "u_fog_col");
    g.u_fog_start     = glGetUniformLocation(g.batch_prog, "u_fog_start");
    g.u_fog_end       = glGetUniformLocation(g.batch_prog, "u_fog_end");
    g.u_has_tex       = glGetUniformLocation(g.batch_prog, "u_has_tex");

    /* --- blit shader --- */
    vs = compile_shader(GL_VERTEX_SHADER, s_vblit);
    fs = compile_shader(GL_FRAGMENT_SHADER, s_fblit);
    if (!vs || !fs) return 0;
    g.blit_prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!g.blit_prog) return 0;

    g.blit_a_pos = glGetAttribLocation(g.blit_prog, "a_pos");
    g.blit_u_tex = glGetUniformLocation(g.blit_prog, "u_tex");

    /* --- blit VBO (full-screen quad) --- */
    static const float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
    };
    glGenBuffers(1, &g.blit_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.blit_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* --- defaults --- */
    g.vp_x = 0; g.vp_y = 0; g.vp_w = GR_FB_W; g.vp_h = GR_FB_H;
    g.state.alpha_blend    = GR_BLEND_NONE;
    g.state.alpha_test     = GR_ALPHATEST_DISABLE;
    g.state.alpha_ref      = 0.0f;
    g.state.col_combine    = GR_COMBINE_MODE_MODULATE;
    g.state.alpha_combine  = GR_COMBINE_MODE_MODULATE;
    g.state.fog_mode       = GR_FOG_DISABLE;
    g.state.fog_col        = 0;
    g.state.fog_start      = 0.0f;
    g.state.fog_end        = 1.0f;
    g.state.cull           = GR_CULL_DISABLE;
    g.state.bound_tex      = -1;

    return 1;
}

void grShutdown(void)
{
    batch_flush();

    glDeleteBuffers(GR_VBO_COUNT, g.vbos);
    glDeleteBuffers(1, &g.blit_vbo);
    glDeleteProgram(g.batch_prog);
    glDeleteProgram(g.blit_prog);
    glDeleteFramebuffers(1, &g.fbo);
    glDeleteTextures(1, &g.colour_tex);
    glDeleteRenderbuffers(1, &g.depth_rbo);

    for (int i = 0; i < GR_MAX_TEXTURES; i++) {
        if (g.textures[i].used)
            glDeleteTextures(1, &g.textures[i].id);
    }

    free(g.batch.data);
    memset(&g, 0, sizeof(g));
}

void grBufferClear(GrColor_t colour, GrDepth_t depth)
{
    batch_flush();

    float r, g_, b, a;
    grColorUnpack(colour, &r, &g_, &b, &a);
    float d = (float)depth / 65535.0f;

    glBindFramebuffer(GL_FRAMEBUFFER, g.fbo);
    glViewport(g.vp_x, g.vp_y, g.vp_w, g.vp_h);
    glClearColor(r, g_, b, a);
    glClearDepthf(d);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void grBufferSwap(struct GLFWwindow *window)
{
    batch_flush();

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    g.win_w = fb_w;
    g.win_h = fb_h;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fb_w, fb_h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* aspect-ratio-preserving blit */
    float scale_x = (float)fb_w / (float)GR_FB_W;
    float scale_y = (float)fb_h / (float)GR_FB_H;
    float scale   = scale_x < scale_y ? scale_x : scale_y;
    int   bw      = (int)(GR_FB_W * scale);
    int   bh      = (int)(GR_FB_H * scale);
    int   bx      = (fb_w - bw) / 2;
    int   by      = (fb_h - bh) / 2;

    glViewport(bx, by, bw, bh);

    glUseProgram(g.blit_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.colour_tex);
    glUniform1i(g.blit_u_tex, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindBuffer(GL_ARRAY_BUFFER, g.blit_vbo);
    glEnableVertexAttribArray((GLuint)g.blit_a_pos);
    glVertexAttribPointer((GLuint)g.blit_a_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray((GLuint)g.blit_a_pos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glfwSwapBuffers(window);
}

void grViewport(int x, int y, int width, int height)
{
    batch_flush();
    g.vp_x = x;
    g.vp_y = y;
    g.vp_w = width;
    g.vp_h = height;
}

/* -------------------------------------------------------------------------- */
/* State – any change flushes the batch                                       */
/* -------------------------------------------------------------------------- */

static void state_change(void)
{
    batch_flush();
}

void grAlphaBlend(GrAlphaBlendMode mode)
{
    if (g.state.alpha_blend != mode) {
        state_change();
        g.state.alpha_blend = mode;
    }
}

void grAlphaTest(GrAlphaTestMode mode, float ref)
{
    if (g.state.alpha_test != mode || g.state.alpha_ref != ref) {
        state_change();
        g.state.alpha_test = mode;
        g.state.alpha_ref  = ref;
    }
}

void grColorCombine(GrCombineMode mode)
{
    if (g.state.col_combine != mode) {
        state_change();
        g.state.col_combine = mode;
    }
}

void grAlphaCombine(GrCombineMode mode)
{
    if (g.state.alpha_combine != mode) {
        state_change();
        g.state.alpha_combine = mode;
    }
}

void grFogMode(GrFogMode mode)
{
    if (g.state.fog_mode != mode) {
        state_change();
        g.state.fog_mode = mode;
    }
}

void grFogColorValue(GrColor_t colour)
{
    if (g.state.fog_col != colour) {
        state_change();
        g.state.fog_col = colour;
    }
}

void grFogTable(const float *table, int n)
{
    if (!table || n <= 0) return;

    if (n == 1) {
        grFogRange(0.0f, table[0]);
        return;
    }

    grFogRange(table[0], table[n - 1]);
}

void grFogRange(float start, float end)
{
    if (start < 0.0f) start = 0.0f;
    if (end <= start) end = start + 0.001f;

    if (g.state.fog_start != start || g.state.fog_end != end) {
        state_change();
        g.state.fog_start = start;
        g.state.fog_end = end;
    }
}

void grCullMode(GrCullMode mode)
{
    if (g.state.cull != mode) {
        state_change();
        g.state.cull = mode;
    }
}

/* -------------------------------------------------------------------------- */
/* Textures                                                                   */
/* -------------------------------------------------------------------------- */

int grTexAllocate(void)
{
    for (int i = 0; i < GR_MAX_TEXTURES; i++) {
        if (!g.textures[i].used) {
            glGenTextures(1, &g.textures[i].id);
            g.textures[i].used = 1;
            g.textures[i].w = 0;
            g.textures[i].h = 0;
            return i;
        }
    }
    return -1;
}

void grTexFree(int tex)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES) return;
    if (!g.textures[tex].used) return;

    if (g.state.bound_tex == tex) {
        state_change();
        g.state.bound_tex = -1;
    }

    glDeleteTextures(1, &g.textures[tex].id);
    g.textures[tex].used = 0;
    g.textures[tex].id   = 0;
}

static void convert_rgb565(const uint16_t *src, uint8_t *dst, int n)
{
    for (int i = 0; i < n; i++) {
        uint16_t p = src[i];
        int r = (p >> 11) & 0x1F;
        int g = (p >>  5) & 0x3F;
        int b =  p        & 0x1F;
        dst[i*4+0] = (uint8_t)((r << 3) | (r >> 2));
        dst[i*4+1] = (uint8_t)((g << 2) | (g >> 4));
        dst[i*4+2] = (uint8_t)((b << 3) | (b >> 2));
        dst[i*4+3] = 255;
    }
}

static void convert_argb1555(const uint16_t *src, uint8_t *dst, int n)
{
    for (int i = 0; i < n; i++) {
        uint16_t p = src[i];
        int a = (p >> 15) & 1;
        int r = (p >> 10) & 0x1F;
        int g = (p >>  5) & 0x1F;
        int b =  p        & 0x1F;
        dst[i*4+0] = (uint8_t)((r << 3) | (r >> 2));
        dst[i*4+1] = (uint8_t)((g << 3) | (g >> 2));
        dst[i*4+2] = (uint8_t)((b << 3) | (b >> 2));
        dst[i*4+3] = a ? 255 : 0;
    }
}

void grTexDownloadMipMap(int tex, const void *data, int w, int h,
                         GrTextureFormat fmt)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g.textures[tex].used) return;
    if (w < 1 || h < 1 || w > GR_MAX_TEX_SIZE || h > GR_MAX_TEX_SIZE) return;
    if (!data) return;

    uint8_t *rgba = NULL;
    const void *upload = data;

    if (fmt == GR_TEXFMT_RGB_565) {
        rgba = (uint8_t *)malloc((size_t)w * h * 4);
        convert_rgb565((const uint16_t *)data, rgba, w * h);
        upload = rgba;
    } else if (fmt == GR_TEXFMT_ARGB_1555) {
        rgba = (uint8_t *)malloc((size_t)w * h * 4);
        convert_argb1555((const uint16_t *)data, rgba, w * h);
        upload = rgba;
    }
    /* GR_TEXFMT_ARGB_8888 passes through */

    glBindTexture(GL_TEXTURE_2D, g.textures[tex].id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, upload);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    g.textures[tex].w = w;
    g.textures[tex].h = h;

    free(rgba);
}

void grTexBind(int tex)
{
    if (tex >= GR_MAX_TEXTURES) tex = -1;
    if (tex >= 0 && !g.textures[tex].used) tex = -1;

    if (g.state.bound_tex != tex) {
        state_change();
        g.state.bound_tex = tex;
    }
}

void grTexFilter(int tex, GrMipMapMode mm, GrTextureFilter minf,
                 GrTextureFilter magf)
{
    if (tex < 0 || tex >= GR_MAX_TEXTURES || !g.textures[tex].used) return;

    GLenum min_gl, mag_gl;

    mag_gl = (magf == GR_TEXTUREFILTER_BILINEAR) ? GL_LINEAR : GL_NEAREST;

    if (mm == GR_MIPMAP_DISABLE) {
        min_gl = (minf == GR_TEXTUREFILTER_BILINEAR) ? GL_LINEAR : GL_NEAREST;
    } else if (mm == GR_MIPMAP_NEAREST) {
        min_gl = (minf == GR_TEXTUREFILTER_BILINEAR)
                 ? GL_LINEAR_MIPMAP_NEAREST
                 : GL_NEAREST_MIPMAP_NEAREST;
    } else {
        min_gl = (minf == GR_TEXTUREFILTER_BILINEAR)
                 ? GL_LINEAR_MIPMAP_LINEAR
                 : GL_NEAREST_MIPMAP_LINEAR;
    }

    glBindTexture(GL_TEXTURE_2D, g.textures[tex].id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_gl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_gl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* -------------------------------------------------------------------------- */
/* Drawing                                                                    */
/* -------------------------------------------------------------------------- */

void grDrawTriangle(const GrVertex *v0,
                    const GrVertex *v1,
                    const GrVertex *v2)
{
    if (cull_tri(v0, v1, v2)) return;
    batch_push(v0);
    batch_push(v1);
    batch_push(v2);
}

void grDrawPoint(const GrVertex *v)
{
    /* Draw as a tiny quad (2 triangles) so it goes through the same pipeline. */
    float s = 1.0f / 320.0f;  /* ~1 pixel in NDC */
    GrVertex v0 = *v;
    GrVertex v1 = *v; v1.x += s;
    GrVertex v2 = *v; v2.y += s;
    GrVertex v3 = *v; v3.x += s; v3.y += s;
    grDrawTriangle(&v0, &v1, &v2);
    grDrawTriangle(&v2, &v1, &v3);
}

void grDrawLine(const GrVertex *v0, const GrVertex *v1)
{
    /* Draw as a thin quad (2 triangles). */
    float dx = v1->x - v0->x;
    float dy = v1->y - v0->y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1e-6f) {
        grDrawPoint(v0);
        return;
    }
    float nx = -dy / len * (1.0f / 320.0f);
    float ny =  dx / len * (1.0f / 320.0f);

    GrVertex a = *v0; a.x += nx; a.y += ny;
    GrVertex b = *v0; b.x -= nx; b.y -= ny;
    GrVertex c = *v1; c.x += nx; c.y += ny;
    GrVertex d = *v1; d.x -= nx; d.y -= ny;

    grDrawTriangle(&a, &c, &b);
    grDrawTriangle(&b, &c, &d);
}

/* -------------------------------------------------------------------------- */
/* Colour helpers                                                             */
/* -------------------------------------------------------------------------- */

GrColor_t grColorPack(float r, float g, float b, float a)
{
    int ri = (int)(r * 255.0f + 0.5f);
    int gi = (int)(g * 255.0f + 0.5f);
    int bi = (int)(b * 255.0f + 0.5f);
    int ai = (int)(a * 255.0f + 0.5f);
    if (ri < 0) ri = 0; else if (ri > 255) ri = 255;
    if (gi < 0) gi = 0; else if (gi > 255) gi = 255;
    if (bi < 0) bi = 0; else if (bi > 255) bi = 255;
    if (ai < 0) ai = 0; else if (ai > 255) ai = 255;
    return ((uint32_t)ai << 24) | ((uint32_t)bi << 16) | ((uint32_t)gi << 8) | (uint32_t)ri;
}

void grColorUnpack(GrColor_t c, float *r, float *g, float *b, float *a)
{
    *r = ((c      ) & 0xFF) / 255.0f;
    *g = ((c >>  8) & 0xFF) / 255.0f;
    *b = ((c >> 16) & 0xFF) / 255.0f;
    *a = ((c >> 24) & 0xFF) / 255.0f;
}
