#ifndef GL_BINDINGS_H
#define GL_BINDINGS_H

#include <stdbool.h>

#include "pocketpy.h"

typedef struct GlBindingHost {
    void *userdata;
    double (*time_now)(void *userdata);
    double (*delta_time)(void *userdata);
    int (*key_down)(void *userdata, int key);
    int (*key_pressed)(void *userdata, int key);
    int (*mouse_down)(void *userdata, int button);
    int (*mouse_pressed)(void *userdata, int button);
    int (*mouse_released)(void *userdata, int button);
    void (*mouse_position)(void *userdata, float *x, float *y);
    void (*framebuffer_size)(void *userdata, int *w, int *h);
    void (*set_title)(void *userdata, const char *title);
    void (*request_quit)(void *userdata);
} GlBindingHost;

void glBindingsSetHost(const GlBindingHost *host);
bool glBindingsRegister(py_Ref module);

#endif
