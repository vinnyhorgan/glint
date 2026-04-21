#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pocketpy.h"
#include "miniaudio.h"

/* -------------------------------------------------------------------------- */
/* Math helpers                                                               */
/* -------------------------------------------------------------------------- */

static void mat4_identity(float m[16])
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_mul(const float a[16], const float b[16], float r[16])
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r[i*4+j] = a[i*4+0]*b[0*4+j]
                     + a[i*4+1]*b[1*4+j]
                     + a[i*4+2]*b[2*4+j]
                     + a[i*4+3]*b[3*4+j];
        }
    }
}

static void mat4_perspective(float fovy, float aspect, float n, float f,
                             float m[16])
{
    float ff = 1.0f / tanf(fovy * 0.5f);
    m[0]  = ff / aspect; m[1]  = 0;   m[2]  = 0;                      m[3]  = 0;
    m[4]  = 0;           m[5]  = ff;  m[6]  = 0;                      m[7]  = 0;
    m[8]  = 0;           m[9]  = 0;   m[10] = (f+n)/(n-f);            m[11] = -1;
    m[12] = 0;           m[13] = 0;   m[14] = (2.0f*f*n)/(n-f);       m[15] = 0;
}

static void mat4_rotate_x(float a, float m[16])
{
    float c = cosf(a), s = sinf(a);
    mat4_identity(m);
    m[5]  =  c; m[6]  = s;
    m[9]  = -s; m[10] = c;
}

static void mat4_rotate_y(float a, float m[16])
{
    float c = cosf(a), s = sinf(a);
    mat4_identity(m);
    m[0]  = c; m[2]  = -s;
    m[8]  = s; m[10] =  c;
}

static void mat4_translate(float x, float y, float z, float m[16])
{
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_transform(const float m[16],
                           float x, float y, float z, float w,
                           float *ox, float *oy, float *oz, float *ow)
{
    *ox = m[0]*x + m[4]*y + m[8]*z  + m[12]*w;
    *oy = m[1]*x + m[5]*y + m[9]*z  + m[13]*w;
    *oz = m[2]*x + m[6]*y + m[10]*z + m[14]*w;
    *ow = m[3]*x + m[7]*y + m[11]*z + m[15]*w;
}

/* -------------------------------------------------------------------------- */
/* Cube data                                                                  */
/* -------------------------------------------------------------------------- */

static const float kCube[8][3] = {
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

static const int kFaces[6][4] = {
    {0,1,2,3}, {5,4,7,6}, {4,0,3,7}, {1,5,6,2}, {4,5,1,0}, {3,2,6,7},
};

static const float kUV[4][2] = { {0,0}, {1,0}, {1,1}, {0,1} };

static const GrColor_t kFaceCol[6] = {
    0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
    0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF,
};

/* -------------------------------------------------------------------------- */
/* Texture generation                                                         */
/* -------------------------------------------------------------------------- */

static uint8_t *make_checker(int w, int h)
{
    uint8_t *p = (uint8_t *)malloc((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int c = ((x >> 3) + (y >> 3)) & 1 ? 255 : 32;
            p[(y*w+x)*4+0] = (uint8_t)((x*4) & 255);
            p[(y*w+x)*4+1] = (uint8_t)c;
            p[(y*w+x)*4+2] = (uint8_t)((y*4) & 255);
            p[(y*w+x)*4+3] = 255;
        }
    }
    return p;
}

/* -------------------------------------------------------------------------- */
/* Demo state                                                                 */
/* -------------------------------------------------------------------------- */

typedef struct {
    int    tex;
    int    blend_on;
    int    cull_on;
    double last_title;
    int    frames;
} Demo;

/* -------------------------------------------------------------------------- */
/* Drawing                                                                    */
/* -------------------------------------------------------------------------- */

static void draw_cube(const Demo *d, float t)
{
    float proj[16], rx[16], ry[16], tr[16], mv[16], mvp[16], tmp[16];

    mat4_perspective(60.0f * (float)M_PI / 180.0f,
                     320.0f / 240.0f, 0.1f, 100.0f, proj);
    mat4_rotate_x(t * 0.7f, rx);
    mat4_rotate_y(t * 0.5f, ry);
    mat4_translate(0.0f, 0.0f, -2.5f, tr);

    mat4_mul(tr, ry, tmp);
    mat4_mul(tmp, rx, mv);
    mat4_mul(proj, mv, mvp);

    grCullMode(d->cull_on ? GR_CULL_NEGATIVE : GR_CULL_DISABLE);
    grColorCombine(GR_COMBINE_MODE_MODULATE);
    grAlphaCombine(GR_COMBINE_MODE_MODULATE);
    grAlphaBlend(d->blend_on ? GR_BLEND_ALPHA : GR_BLEND_NONE);
    grTexBind(d->tex);

    for (int f = 0; f < 6; f++) {
        float fr, fg, fb, fa;
        grColorUnpack(kFaceCol[f], &fr, &fg, &fb, &fa);

        for (int tri = 0; tri < 2; tri++) {
            const int *idx = kFaces[f];
            int i0 = idx[0];
            int i1 = idx[tri+1];
            int i2 = idx[tri+2];

            GrVertex v[3];
            for (int k = 0; k < 3; k++) {
                int vi = (k==0)?i0:(k==1)?i1:i2;
                float cx, cy, cz, cw;
                mat4_transform(mvp,
                               kCube[vi][0], kCube[vi][1], kCube[vi][2], 1.0f,
                               &cx, &cy, &cz, &cw);
                v[k].x   = cx;
                v[k].y   = cy;
                v[k].z   = cz;
                v[k].oow = 1.0f / cw;
                v[k].r   = fr;
                v[k].g   = fg;
                v[k].b   = fb;
                v[k].a   = fa;
                int uvi = (k==0)?0:(k==1)?(tri+1):(tri+2);
                v[k].u   = kUV[uvi][0];
                v[k].v   = kUV[uvi][1];
            }
            grDrawTriangle(&v[0], &v[1], &v[2]);
        }
    }
}

static void draw_overlay(float t)
{
    grCullMode(GR_CULL_DISABLE);
    grTexBind(-1);
    grColorCombine(GR_COMBINE_MODE_MODULATE);
    grAlphaCombine(GR_COMBINE_MODE_MODULATE);
    grAlphaBlend(GR_BLEND_ALPHA);

    /* spinning triangle in top-left */
    float cx = (30.0f / 160.0f) - 1.0f;
    float cy = (30.0f / 120.0f) - 1.0f;
    float s = 20.0f / 160.0f;
    float c = cosf(t * 3.0f);
    float sn = sinf(t * 3.0f);

    GrVertex v0 = { cx, cy, -0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0, 0 };
    GrVertex v1 = { cx + s*c, cy + s*sn, -0.9f, 1.0f, 1.0f, 0.0f, 0.0f, 0.6f, 0, 0 };
    GrVertex v2 = { cx - s*sn, cy + s*c, -0.9f, 1.0f, 0.0f, 1.0f, 0.0f, 0.6f, 0, 0 };
    grDrawTriangle(&v0, &v1, &v2);

    /* pulsing quad in bottom-right */
    float px = (280.0f / 160.0f) - 1.0f;
    float py = (200.0f / 120.0f) - 1.0f;
    float ps = (8.0f + 4.0f * sinf(t * 4.0f)) / 160.0f;
    GrColor_t col = grColorPack(0.2f, 0.6f, 1.0f, 0.5f);
    float r, g, b, a;
    grColorUnpack(col, &r, &g, &b, &a);

    GrVertex p0 = { px - ps, py - ps, -0.9f, 1.0f, r, g, b, a, 0, 0 };
    GrVertex p1 = { px + ps, py - ps, -0.9f, 1.0f, r, g, b, a, 0, 0 };
    GrVertex p2 = { px - ps, py + ps, -0.9f, 1.0f, r, g, b, a, 0, 0 };
    GrVertex p3 = { px + ps, py + ps, -0.9f, 1.0f, r, g, b, a, 0, 0 };
    grDrawTriangle(&p0, &p1, &p2);
    grDrawTriangle(&p2, &p1, &p3);
}

/* -------------------------------------------------------------------------- */
/* GLFW callbacks                                                             */
/* -------------------------------------------------------------------------- */

static void error_cb(int code, const char *desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

static void key_cb(GLFWwindow *win, int key, int scancode, int action,
                   int mods)
{
    (void)scancode; (void)mods;
    Demo *d = (Demo *)glfwGetWindowUserPointer(win);
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(win, 1);
    else if (key == GLFW_KEY_B)
        d->blend_on = !d->blend_on;
    else if (key == GLFW_KEY_C)
        d->cull_on = !d->cull_on;
}

/* -------------------------------------------------------------------------- */
/* Vendor tests                                                               */
/* -------------------------------------------------------------------------- */

static void test_pocketpy(void)
{
    py_initialize();
    py_GlobalRef mod = py_getmodule("__main__");
    if (!py_exec("print('pocketpy OK')", "<test>", EXEC_MODE, mod))
        py_printexc();
    py_finalize();
}

static void test_miniaudio(void)
{
    ma_engine e;
    if (ma_engine_init(NULL, &e) == MA_SUCCESS) {
        fprintf(stderr, "miniaudio OK\n");
        ma_engine_uninit(&e);
    } else {
        fprintf(stderr, "miniaudio init failed\n");
    }
}

/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

static GLADapiproc glad_loader(const char *name)
{
    return glfwGetProcAddress(name);
}

int main(void)
{
    GLFWwindow *win;
    Demo demo = {0};

    test_pocketpy();
    test_miniaudio();

    glfwSetErrorCallback(error_cb);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    win = glfwCreateWindow(960, 720, "glint – press B=blend C=cull ESC=quit",
                           NULL, NULL);
    if (!win) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    glfwSetWindowUserPointer(win, &demo);
    glfwSetKeyCallback(win, key_cb);
    glfwSwapInterval(1);

    if (!gladLoadGLES2(glad_loader)) {
        fprintf(stderr, "gladLoadGLES2 failed\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    if (!grInit(960, 720)) {
        fprintf(stderr, "grInit failed\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    /* create test texture */
    demo.tex = grTexAllocate();
    if (demo.tex >= 0) {
        uint8_t *chk = make_checker(64, 64);
        grTexDownloadMipMap(demo.tex, chk, 64, 64, GR_TEXFMT_ARGB_8888);
        grTexFilter(demo.tex, GR_MIPMAP_NEAREST,
                    GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
        free(chk);
    }

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float t = (float)now;

        int fw = 0, fh = 0;
        glfwGetFramebufferSize(win, &fw, &fh);

        grBufferClear(0xFF101020, 0xFFFF);
        draw_cube(&demo, t);
        draw_overlay(t);
        grBufferSwap(win);

        /* FPS */
        demo.frames++;
        if (now - demo.last_title >= 1.0) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "glint – %d FPS | B=blend(%s) C=cull(%s) | %dx%d",
                     demo.frames,
                     demo.blend_on ? "on" : "off",
                     demo.cull_on  ? "on" : "off",
                     fw, fh);
            glfwSetWindowTitle(win, buf);
            demo.frames = 0;
            demo.last_title = now;
        }

        glfwPollEvents();
    }

    grTexFree(demo.tex);
    grShutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
