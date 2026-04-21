#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl.h"
#include "gl_core.h"
#include "gl_bindings.h"

#include <stdio.h>
#include <stdlib.h>

#include "pocketpy.h"

#define DEFAULT_SCRIPT_PATH "scripts/demo.py"

typedef struct {
    GLFWwindow *window;
    double time_now;
    double delta_time;
} RuntimeHost;

static int g_needs_resize_repaint;

static void present_black(GLFWwindow *window)
{
    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fb_w, fb_h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
}

static void error_cb(int code, const char *desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

static GLADapiproc glad_loader(const char *name)
{
    return glfwGetProcAddress(name);
}

static double host_time_now(void *userdata)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    return host->time_now;
}

static double host_delta_time(void *userdata)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    return host->delta_time;
}

static int host_key_down(void *userdata, int key)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    return glfwGetKey(host->window, key) == GLFW_PRESS;
}

static int host_mouse_down(void *userdata, int button)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    return glfwGetMouseButton(host->window, button) == GLFW_PRESS;
}

static void host_mouse_position(void *userdata, float *x, float *y)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    int win_w = 0;
    int win_h = 0;
    int view_x = 0;
    int view_y = 0;
    int view_w = 0;
    int view_h = 0;
    double mx = 0.0;
    double my = 0.0;

    glfwGetWindowSize(host->window, &win_w, &win_h);
    glfwGetCursorPos(host->window, &mx, &my);

    grCoreComputeBlitRect(win_w, win_h, &view_x, &view_y, &view_w, &view_h);
    if (view_w <= 0 || view_h <= 0) {
        *x = 0.0f;
        *y = 0.0f;
        return;
    }

    mx = (mx - (double)view_x) * (double)GR_FB_W / (double)view_w;
    my = (my - (double)view_y) * (double)GR_FB_H / (double)view_h;
    if (mx < 0.0)
        mx = 0.0;
    if (my < 0.0)
        my = 0.0;
    if (mx > (double)(GR_FB_W - 1))
        mx = (double)(GR_FB_W - 1);
    if (my > (double)(GR_FB_H - 1))
        my = (double)(GR_FB_H - 1);

    *x = (float)mx;
    *y = (float)my;
}

static void host_framebuffer_size(void *userdata, int *w, int *h)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    glfwGetFramebufferSize(host->window, w, h);
}

static void host_set_title(void *userdata, const char *title)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    glfwSetWindowTitle(host->window, title);
}

static void host_request_quit(void *userdata)
{
    RuntimeHost *host = (RuntimeHost *)userdata;
    glfwSetWindowShouldClose(host->window, 1);
}

static void key_cb(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, 1);
}

static void framebuffer_size_cb(GLFWwindow *window, int width, int height)
{
    (void)window;
    (void)width;
    (void)height;
    g_needs_resize_repaint = 1;
}

static void window_refresh_cb(GLFWwindow *window)
{
    (void)window;
    g_needs_resize_repaint = 1;
}

static char *read_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char *buf;
    long size;
    size_t nread;

    if (fp == NULL)
        return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 1u);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    nread = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (nread != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    return buf;
}

static int call_python0(py_Ref func, const char *label)
{
    if (!py_call(func, 0, NULL)) {
        fprintf(stderr, "%s failed\n", label);
        py_printexc();
        return 0;
    }
    return 1;
}

static int call_python1f(py_Ref func, float value, const char *label)
{
    py_TValue arg;
    py_newfloat(&arg, value);
    if (!py_call(func, 1, &arg)) {
        fprintf(stderr, "%s failed\n", label);
        py_printexc();
        return 0;
    }
    return 1;
}

static int load_runtime_script(const char *path, py_Ref module)
{
    char *source = read_text_file(path);
    if (source == NULL) {
        fprintf(stderr, "failed to read script: %s\n", path);
        return 0;
    }
    if (!py_exec(source, path, EXEC_MODE, module)) {
        fprintf(stderr, "failed to execute script: %s\n", path);
        py_printexc();
        free(source);
        return 0;
    }
    free(source);
    return 1;
}

int main(int argc, char **argv)
{
    const char *script_path = argc > 1 ? argv[1] : DEFAULT_SCRIPT_PATH;
    GLFWwindow *window;
    RuntimeHost host = {0};
    GlBindingHost binding_host = {0};
    py_GlobalRef glide;
    py_GlobalRef main_mod;
    py_TValue load_func;
    py_TValue update_func;
    py_TValue draw_func;
    double last_time;
    int exit_code = 1;

    glfwSetErrorCallback(error_cb);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);

    window = glfwCreateWindow(960, 720, "glint", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowOpacity(window, 1.0f);
    glfwSetKeyCallback(window, key_cb);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetWindowRefreshCallback(window, window_refresh_cb);
    glfwSwapInterval(1);

    if (!gladLoadGLES2(glad_loader)) {
        fprintf(stderr, "gladLoadGLES2 failed\n");
        goto cleanup_window;
    }

    if (!grInit(960, 720)) {
        fprintf(stderr, "grInit failed\n");
        goto cleanup_window;
    }

    host.window = window;
    host.time_now = glfwGetTime();
    binding_host.userdata = &host;
    binding_host.time_now = host_time_now;
    binding_host.delta_time = host_delta_time;
    binding_host.key_down = host_key_down;
    binding_host.mouse_down = host_mouse_down;
    binding_host.mouse_position = host_mouse_position;
    binding_host.framebuffer_size = host_framebuffer_size;
    binding_host.set_title = host_set_title;
    binding_host.request_quit = host_request_quit;

    py_initialize();
    py_sys_setargv(argc, argv);
    glide = py_newmodule("glide");
    glBindingsSetHost(&binding_host);
    if (!glBindingsRegister(glide)) {
        py_printexc();
        goto cleanup_python;
    }

    main_mod = py_getmodule("__main__");
    if (!load_runtime_script(script_path, main_mod))
        goto cleanup_python;

    if (!py_getattr(main_mod, py_name("load"))) {
        fprintf(stderr, "script must define load()\n");
        py_printexc();
        goto cleanup_python;
    }
    py_assign(&load_func, py_retval());
    if (!py_getattr(main_mod, py_name("update"))) {
        fprintf(stderr, "script must define update(dt)\n");
        py_printexc();
        goto cleanup_python;
    }
    py_assign(&update_func, py_retval());
    if (!py_getattr(main_mod, py_name("draw"))) {
        fprintf(stderr, "script must define draw()\n");
        py_printexc();
        goto cleanup_python;
    }
    py_assign(&draw_func, py_retval());

    if (!call_python0(&load_func, "load()"))
        goto cleanup_python;

    last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now;

        glfwPollEvents();
        if (g_needs_resize_repaint) {
            present_black(window);
            g_needs_resize_repaint = 0;
        }
        now = glfwGetTime();
        host.delta_time = now - last_time;
        if (host.delta_time > 0.1)
            host.delta_time = 0.1;
        host.time_now = now;
        last_time = now;

        grViewport(0, 0, GR_FB_W, GR_FB_H);
        grClipWindow(0, 0, GR_FB_W, GR_FB_H);

        if (!call_python1f(&update_func, (float)host.delta_time, "update(dt)"))
            goto cleanup_python;
        if (!call_python0(&draw_func, "draw()"))
            goto cleanup_python;

        grBufferSwap(window);
    }

    exit_code = 0;

cleanup_python:
    py_finalize();
    glBindingsSetHost(NULL);
    grShutdown();
cleanup_window:
    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
