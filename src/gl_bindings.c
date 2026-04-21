#include "gl_bindings.h"

#include "gl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static GlBindingHost g_host;

static const char *kBindingsBootstrap[] = {
    "def _alpha_byte(alpha):\n"
    "    if alpha <= 0.0:\n"
    "        return 0\n"
    "    if alpha >= 1.0:\n"
    "        return 255\n"
    "    return int(alpha * 255.0 + 0.5)\n"
    "\n"
    "def _unpack_rgba(color):\n"
    "    if isinstance(color, int):\n"
    "        return color_unpack(color)\n"
    "    if len(color) == 3:\n"
    "        return (color[0], color[1], color[2], 1.0)\n"
    "    if len(color) == 4:\n"
    "        return (color[0], color[1], color[2], color[3])\n"
    "    raise ValueError('color must be a packed int or a 3/4-item sequence')\n"
    "\n"
    "def rgba(r, g, b, a=1.0):\n"
    "    return color_pack(r, g, b, a)\n"
    "\n"
    "def rgb(r, g, b):\n"
    "    return color_pack(r, g, b, 1.0)\n"
    "\n"
    "def color_tuple(color):\n"
    "    return _unpack_rgba(color)\n"
    "\n"
    "class Vertex:\n"
    "    def __init__(self, x, y, z=0.0, oow=1.0, color=(1.0, 1.0, 1.0, 1.0), u=0.0, v=0.0):\n"
    "        r, g, b, a = _unpack_rgba(color)\n"
    "        self.x = x\n"
    "        self.y = y\n"
    "        self.z = z\n"
    "        self.oow = oow\n"
    "        self.r = r\n"
    "        self.g = g\n"
    "        self.b = b\n"
    "        self.a = a\n"
    "        self.u = u\n"
    "        self.v = v\n"
    "\n"
    "    def as_tuple(self):\n"
    "        return (self.x, self.y, self.z, self.oow, self.r, self.g, self.b, self.a, self.u, self.v)\n"
    "\n"
    "    def copy(self):\n"
    "        return Vertex(self.x, self.y, self.z, self.oow, (self.r, self.g, self.b, self.a), self.u, self.v)\n"
    "\n"
    "def vertex(x, y, z=0.0, oow=1.0, color=(1.0, 1.0, 1.0, 1.0), u=0.0, v=0.0):\n"
    "    return Vertex(x, y, z, oow, color, u, v)\n"
    "\n"
    "def _color_component(value):\n"
    "    if value > 1.0 or value < 0.0:\n"
    "        return value / 255.0\n"
    "    return value\n"
    "\n"
    "def _coerce_vertex(v):\n"
    "    if isinstance(v, Vertex):\n"
    "        return v.as_tuple()\n"
    "    n = len(v)\n"
    "    if n == 2:\n"
    "        return (v[0], v[1], 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0)\n"
    "    if n == 4:\n"
    "        return (v[0], v[1], 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, v[2], v[3])\n"
    "    if n == 5:\n"
    "        return (v[0], v[1], 0.0, 1.0, _color_component(v[2]), _color_component(v[3]), _color_component(v[4]), 1.0, 0.0, 0.0)\n"
    "    if n == 7:\n"
    "        return (v[0], v[1], 0.0, 1.0, _color_component(v[2]), _color_component(v[3]), _color_component(v[4]), 1.0, v[5], v[6])\n"
    "    if n == 8:\n"
    "        return (v[0], v[1], v[2], 1.0, _color_component(v[3]), _color_component(v[4]), _color_component(v[5]), 1.0, v[6], v[7])\n"
    "    if n == 10:\n"
    "        return v\n"
    "    raise ValueError('vertex must be glide.Vertex or a 2/4/5/7/8/10-item sequence')\n"
    "\n"
    "def clear(color=(0.0, 0.0, 0.0, 1.0), depth=0xFFFF):\n"
    "    r, g, b, a = _unpack_rgba(color)\n"
    "    buffer_clear(color_pack(r, g, b, a), _alpha_byte(a), depth)\n"
    "\n"
    "def swap():\n"
    "    buffer_swap_current()\n"
    "\n"
    "def triangle(v0, v1, v2):\n"
    "    draw_triangle(_coerce_vertex(v0), _coerce_vertex(v1), _coerce_vertex(v2))\n"
    "\n"
    "def point(v):\n"
    "    draw_point(_coerce_vertex(v))\n"
    "\n"
    "def line(v0, v1):\n"
    "    draw_line(_coerce_vertex(v0), _coerce_vertex(v1))\n"
    "\n"
,
    "def quad(v0, v1, v2, v3):\n"
    "    triangle(v0, v1, v2)\n"
    "    triangle(v0, v2, v3)\n"
    "\n"
    "def rect(x, y, w, h, color=(1.0, 1.0, 1.0, 1.0), z=0.0):\n"
    "    v0 = vertex(x, y, z, 1.0, color, 0.0, 0.0)\n"
    "    v1 = vertex(x + w, y, z, 1.0, color, 1.0, 0.0)\n"
    "    v2 = vertex(x + w, y + h, z, 1.0, color, 1.0, 1.0)\n"
    "    v3 = vertex(x, y + h, z, 1.0, color, 0.0, 1.0)\n"
    "    quad(v0, v1, v2, v3)\n"
    "\n"
    "def image(tex, x, y, w, h, color=(1.0, 1.0, 1.0, 1.0), z=0.0, u0=0.0, v0=0.0, u1=1.0, v1=1.0):\n"
    "    set_textured_modulate()\n"
    "    tex_bind(tex)\n"
    "    q0 = vertex(x, y, z, 1.0, color, u0, v0)\n"
    "    q1 = vertex(x + w, y, z, 1.0, color, u1, v0)\n"
    "    q2 = vertex(x + w, y + h, z, 1.0, color, u1, v1)\n"
    "    q3 = vertex(x, y + h, z, 1.0, color, u0, v1)\n"
    "    quad(q0, q1, q2, q3)\n"
    "\n"
    "def upload_texture(width, height, pixels, fmt=None, mipmap=None, min_filter=None, mag_filter=None, s_clamp=None, t_clamp=None):\n"
    "    if fmt is None:\n"
    "        fmt = TEXFMT_ARGB_8888\n"
    "    if mipmap is None:\n"
    "        mipmap = MIPMAP_DISABLE\n"
    "    if min_filter is None:\n"
    "        min_filter = TEXTUREFILTER_POINT_SAMPLED\n"
    "    if s_clamp is None:\n"
    "        s_clamp = TEXTURECLAMP_WRAP\n"
    "    if mag_filter is None:\n"
    "        mag_filter = min_filter\n"
    "    if t_clamp is None:\n"
    "        t_clamp = s_clamp\n"
    "    tex = tex_allocate()\n"
    "    tex_download_mipmap(tex, width, height, fmt, pixels)\n"
    "    tex_filter(tex, mipmap, min_filter, mag_filter)\n"
    "    tex_clamp_mode(tex, s_clamp, t_clamp)\n"
    "    return tex\n"
    "\n"
    "def make_fog_table(start_w, end_w):\n"
    "    if end_w <= start_w:\n"
    "        raise ValueError('end_w must be greater than start_w')\n"
    "    out = []\n"
    "    for i in range(FOG_TABLE_SIZE):\n"
    "        w = (2.0 ** (3.0 + (i >> 2))) / (8.0 - (i & 3))\n"
    "        t = (w - start_w) / (end_w - start_w)\n"
    "        if t < 0.0:\n"
    "            t = 0.0\n"
    "        elif t > 1.0:\n"
    "            t = 1.0\n"
    "        out.append(int(t * 255.0 + 0.5))\n"
    "    return out\n"
    "\n"
    "def set_untextured():\n"
    "    tex_bind(-1)\n"
    "    color_combine(COMBINE_FUNCTION_LOCAL, COMBINE_FACTOR_NONE, COMBINE_LOCAL_ITERATED, COMBINE_OTHER_NONE, False)\n"
    "    alpha_combine(COMBINE_FUNCTION_LOCAL, COMBINE_FACTOR_NONE, COMBINE_LOCAL_ITERATED, COMBINE_OTHER_NONE, False)\n"
    "\n"
    "def set_textured_modulate():\n"
    "    color_combine(COMBINE_FUNCTION_SCALE_OTHER, COMBINE_FACTOR_LOCAL, COMBINE_LOCAL_ITERATED, COMBINE_OTHER_TEXTURE, False)\n"
    "    alpha_combine(COMBINE_FUNCTION_SCALE_OTHER, COMBINE_FACTOR_LOCAL_ALPHA, COMBINE_LOCAL_ITERATED, COMBINE_OTHER_TEXTURE, False)\n"
    "\n"
    "def set_blend_none():\n"
    "    alpha_blend_function(BLEND_ONE, BLEND_ZERO, BLEND_ONE, BLEND_ZERO)\n"
    "\n"
    "def set_blend_alpha():\n"
    "    alpha_blend_function(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA, BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA)\n"
    "\n"
    "def set_blend_add():\n"
    "    alpha_blend_function(BLEND_SRC_ALPHA, BLEND_ONE, BLEND_ONE, BLEND_ONE)\n"
    "\n",
    "def set_mode(mode):\n"
    "    if mode == 'flat':\n"
    "        tex_bind(-1)\n"
    "        color_combine(COMBINE_FUNCTION_LOCAL, COMBINE_FACTOR_NONE, COMBINE_LOCAL_CONSTANT, COMBINE_OTHER_NONE, False)\n"
    "        alpha_combine(COMBINE_FUNCTION_LOCAL, COMBINE_FACTOR_NONE, COMBINE_LOCAL_CONSTANT, COMBINE_OTHER_NONE, False)\n"
    "        set_blend_none()\n"
    "        return\n"
    "    if mode == 'gouraud':\n"
    "        set_untextured()\n"
    "        set_blend_none()\n"
    "        return\n"
    "    if mode == 'textured':\n"
    "        color_combine(COMBINE_FUNCTION_SCALE_OTHER, COMBINE_FACTOR_LOCAL, COMBINE_LOCAL_CONSTANT, COMBINE_OTHER_TEXTURE, False)\n"
    "        alpha_combine(COMBINE_FUNCTION_SCALE_OTHER, COMBINE_FACTOR_LOCAL_ALPHA, COMBINE_LOCAL_CONSTANT, COMBINE_OTHER_TEXTURE, False)\n"
    "        set_blend_none()\n"
    "        return\n"
    "    if mode == 'textured_gouraud':\n"
    "        set_textured_modulate()\n"
    "        set_blend_none()\n"
    "        return\n"
    "    if mode == 'transparent':\n"
    "        set_textured_modulate()\n"
    "        set_blend_alpha()\n"
    "        return\n"
    "    raise ValueError('unknown mode: ' + str(mode))\n"
    "\n",
    "def begin_2d():\n"
    "    viewport(0, 0, FB_W, FB_H)\n"
    "    clip_window(0, 0, FB_W, FB_H)\n"
    "    depth_buffer_mode(DEPTHBUFFER_DISABLE)\n"
    "    depth_mask(False)\n"
    "\n"
    "def begin_3d():\n"
    "    viewport(0, 0, FB_W, FB_H)\n"
    "    clip_window(0, 0, FB_W, FB_H)\n"
    "    depth_buffer_mode(DEPTHBUFFER_ZBUFFER)\n"
    "    depth_buffer_function(CMP_LESS)\n"
    "    depth_mask(True)\n"
    "\n",
    "def mouse_x():\n"
    "    return mouse_position()[0]\n"
    "\n"
    "def mouse_y():\n"
    "    return mouse_position()[1]\n",
};

static bool exec_bootstrap(py_Ref module)
{
    size_t total = 1;
    size_t i;
    char *src;
    char *dst;

    for (i = 0; i < sizeof(kBindingsBootstrap) / sizeof(kBindingsBootstrap[0]); ++i)
        total += strlen(kBindingsBootstrap[i]);
    src = (char *)malloc(total);
    if (src == NULL)
        return false;
    dst = src;
    for (i = 0; i < sizeof(kBindingsBootstrap) / sizeof(kBindingsBootstrap[0]); ++i) {
        size_t len = strlen(kBindingsBootstrap[i]);
        memcpy(dst, kBindingsBootstrap[i], len);
        dst += len;
    }
    *dst = '\0';
    if (!py_exec(src, "<glide-bootstrap>", EXEC_MODE, module)) {
        free(src);
        return false;
    }
    free(src);
    return true;
}

void glBindingsSetHost(const GlBindingHost *host)
{
    if (host != NULL)
        g_host = *host;
    else
        memset(&g_host, 0, sizeof(g_host));
}

static bool cast_float_arg(int index, py_StackRef argv, float *out)
{
    return py_castfloat32(py_arg(index), out);
}

static bool cast_int_arg(int index, py_StackRef argv, int *out)
{
    py_i64 value = 0;
    if (!py_castint(py_arg(index), &value))
        return false;
    *out = (int)value;
    return true;
}

static void set_module_int(py_Ref module, const char *name, int value)
{
    py_TValue tmp;
    py_newint(&tmp, value);
    py_setdict(module, py_name(name), &tmp);
}

static int seq_len(py_Ref obj)
{
    if (py_islist(obj))
        return py_list_len(obj);
    if (py_istuple(obj))
        return py_tuple_len(obj);
    return -1;
}

static py_Ref seq_getitem(py_Ref obj, int index)
{
    if (py_islist(obj))
        return py_list_getitem(obj, index);
    if (py_istuple(obj))
        return py_tuple_getitem(obj, index);
    return py_None();
}

static bool cast_float_ref(py_Ref value, float *out)
{
    return py_castfloat32(value, out);
}

static bool cast_int_ref(py_Ref value, int *out)
{
    py_i64 i = 0;
    if (!py_isint(value) && !py_isbool(value))
        return false;
    if (!py_castint(value, &i))
        return false;
    *out = (int)i;
    return true;
}

static int parse_key_code(py_Ref value, int *out)
{
    const char *s;
    if (cast_int_ref(value, out))
        return 1;
    if (!py_isstr(value))
        return 0;
    s = py_tostr(value);
    if (strlen(s) == 1) {
        char c = s[0];
        if (c >= 'a' && c <= 'z') {
            *out = 'A' + (c - 'a');
            return 1;
        }
        if (c >= 'A' && c <= 'Z') {
            *out = c;
            return 1;
        }
        if (c >= '0' && c <= '9') {
            *out = c;
            return 1;
        }
    }
    if (strcmp(s, "space") == 0) *out = 32;
    else if (strcmp(s, "enter") == 0 || strcmp(s, "return") == 0) *out = 257;
    else if (strcmp(s, "escape") == 0 || strcmp(s, "esc") == 0) *out = 256;
    else if (strcmp(s, "tab") == 0) *out = 258;
    else if (strcmp(s, "left") == 0) *out = 263;
    else if (strcmp(s, "right") == 0) *out = 262;
    else if (strcmp(s, "up") == 0) *out = 265;
    else if (strcmp(s, "down") == 0) *out = 264;
    else if (strcmp(s, "lshift") == 0 || strcmp(s, "shift") == 0) *out = 340;
    else if (strcmp(s, "rshift") == 0) *out = 344;
    else if (strcmp(s, "lctrl") == 0 || strcmp(s, "ctrl") == 0) *out = 341;
    else if (strcmp(s, "rctrl") == 0) *out = 345;
    else if (strcmp(s, "lalt") == 0 || strcmp(s, "alt") == 0) *out = 342;
    else if (strcmp(s, "ralt") == 0) *out = 346;
    else return 0;
    return 1;
}

static int parse_mouse_button(py_Ref value, int *out)
{
    const char *s;
    if (cast_int_ref(value, out))
        return 1;
    if (!py_isstr(value))
        return 0;
    s = py_tostr(value);
    if (strcmp(s, "left") == 0) *out = 0;
    else if (strcmp(s, "right") == 0) *out = 1;
    else if (strcmp(s, "middle") == 0) *out = 2;
    else return 0;
    return 1;
}

static bool parse_vertex(py_Ref obj, GrVertex *out)
{
    float values[10];
    int len = seq_len(obj);
    int i;
    memset(out, 0, sizeof(*out));
    out->oow = 1.0f;
    out->r = 1.0f;
    out->g = 1.0f;
    out->b = 1.0f;
    out->a = 1.0f;
    if (len < 0)
        return TypeError("expected a list or tuple for a vertex");
    if (len != 2 && len != 4 && len != 5 && len != 7 && len != 8 && len != 10)
        return ValueError("expected a 2/4/5/7/8/10-item vertex sequence, got %d items", len);
    for (i = 0; i < len; ++i) {
        if (!cast_float_ref(seq_getitem(obj, i), &values[i]))
            return TypeError("vertex element %d must be numeric", i);
    }

    out->x = values[0];
    out->y = values[1];
    if (len == 2)
        return true;
    if (len == 4) {
        out->u = values[2];
        out->v = values[3];
        return true;
    }
    if (len == 5 || len == 7 || len == 8) {
        int color_index = len == 8 ? 3 : 2;
        out->r = values[color_index + 0] > 1.0f ? values[color_index + 0] / 255.0f : values[color_index + 0];
        out->g = values[color_index + 1] > 1.0f ? values[color_index + 1] / 255.0f : values[color_index + 1];
        out->b = values[color_index + 2] > 1.0f ? values[color_index + 2] / 255.0f : values[color_index + 2];
        if (len == 8)
            out->z = values[2];
        if (len == 7) {
            out->u = values[5];
            out->v = values[6];
        } else if (len == 8) {
            out->u = values[6];
            out->v = values[7];
        }
        return true;
    }

    out->z = values[2];
    out->oow = values[3];
    out->r = values[4];
    out->g = values[5];
    out->b = values[6];
    out->a = values[7];
    out->u = values[8];
    out->v = values[9];
    return true;
}

static bool parse_byte_list(py_Ref obj, uint8_t *out, int len)
{
    int size;
    int i;
    if (py_istype(obj, tp_bytes)) {
        unsigned char *data = py_tobytes(obj, &size);
        if (size != len)
            return ValueError("expected %d bytes, got %d", len, size);
        memcpy(out, data, (size_t)len);
        return true;
    }
    size = seq_len(obj);
    if (size < 0)
        return TypeError("expected bytes, list, or tuple");
    if (size != len)
        return ValueError("expected %d items, got %d", len, size);
    for (i = 0; i < len; ++i) {
        int value;
        if (!cast_int_ref(seq_getitem(obj, i), &value) || value < 0 || value > 255)
            return ValueError("byte element %d must be in range 0..255", i);
        out[i] = (uint8_t)value;
    }
    return true;
}

static bool parse_u16_list(py_Ref obj, uint16_t *out, int len)
{
    int size;
    int i;
    size = seq_len(obj);
    if (size < 0)
        return TypeError("expected a list or tuple of 16-bit integers");
    if (size != len)
        return ValueError("expected %d items, got %d", len, size);
    for (i = 0; i < len; ++i) {
        int value;
        if (!cast_int_ref(seq_getitem(obj, i), &value) || value < 0 || value > 0xFFFF)
            return ValueError("16-bit element %d must be in range 0..65535", i);
        out[i] = (uint16_t)value;
    }
    return true;
}

static bool py_color_pack(int argc, py_StackRef argv)
{
    float r, g, b, a;
    (void)argc;
    if (!cast_float_arg(0, argv, &r) || !cast_float_arg(1, argv, &g)
        || !cast_float_arg(2, argv, &b) || !cast_float_arg(3, argv, &a))
        return false;
    py_newint(py_retval(), grColorPack(r, g, b, a));
    return true;
}

static bool py_color_unpack(int argc, py_StackRef argv)
{
    int color;
    float r, g, b, a;
    py_ObjectRef tuple;
    (void)argc;
    if (!cast_int_arg(0, argv, &color))
        return false;
    grColorUnpack((GrColor_t)color, &r, &g, &b, &a);
    tuple = py_newtuple(py_retval(), 4);
    py_newfloat(py_tuple_getitem(py_retval(), 0), r);
    py_newfloat(py_tuple_getitem(py_retval(), 1), g);
    py_newfloat(py_tuple_getitem(py_retval(), 2), b);
    py_newfloat(py_tuple_getitem(py_retval(), 3), a);
    (void)tuple;
    return true;
}

static bool py_init_renderer(int argc, py_StackRef argv)
{
    int w, h;
    (void)argc;
    if (!cast_int_arg(0, argv, &w) || !cast_int_arg(1, argv, &h))
        return false;
    py_newbool(py_retval(), grInit(w, h) != 0);
    return true;
}

static bool py_shutdown_renderer(int argc, py_StackRef argv)
{
    (void)argc;
    (void)argv;
    grShutdown();
    py_newnone(py_retval());
    return true;
}

static bool py_buffer_clear(int argc, py_StackRef argv)
{
    int color, alpha, depth;
    (void)argc;
    if (!cast_int_arg(0, argv, &color) || !cast_int_arg(1, argv, &alpha) || !cast_int_arg(2, argv, &depth))
        return false;
    grBufferClear((GrColor_t)color, (GrAlpha_t)alpha, (GrDepth_t)depth);
    py_newnone(py_retval());
    return true;
}

static bool py_buffer_swap_current(int argc, py_StackRef argv)
{
    (void)argc;
    (void)argv;
    grBufferSwapCurrent();
    py_newnone(py_retval());
    return true;
}

static bool py_time_now(int argc, py_StackRef argv)
{
    double value = 0.0;
    (void)argc;
    (void)argv;
    if (g_host.time_now != NULL)
        value = g_host.time_now(g_host.userdata);
    py_newfloat(py_retval(), value);
    return true;
}

static bool py_delta_time(int argc, py_StackRef argv)
{
    double value = 0.0;
    (void)argc;
    (void)argv;
    if (g_host.delta_time != NULL)
        value = g_host.delta_time(g_host.userdata);
    py_newfloat(py_retval(), value);
    return true;
}

static bool py_key_down(int argc, py_StackRef argv)
{
    int key = 0;
    (void)argc;
    if (!parse_key_code(py_arg(0), &key))
        return TypeError("key must be a keycode int or key name string");
    py_newbool(py_retval(), g_host.key_down != NULL && g_host.key_down(g_host.userdata, key));
    return true;
}

static bool py_key_pressed(int argc, py_StackRef argv)
{
    int key = 0;
    (void)argc;
    if (!parse_key_code(py_arg(0), &key))
        return TypeError("key must be a keycode int or key name string");
    py_newbool(py_retval(), g_host.key_pressed != NULL && g_host.key_pressed(g_host.userdata, key));
    return true;
}

static bool py_mouse_down(int argc, py_StackRef argv)
{
    int button = 0;
    (void)argc;
    if (!parse_mouse_button(py_arg(0), &button))
        return TypeError("button must be an int or 'left'/'right'/'middle'");
    py_newbool(py_retval(), g_host.mouse_down != NULL && g_host.mouse_down(g_host.userdata, button));
    return true;
}

static bool py_mouse_position(int argc, py_StackRef argv)
{
    float x = 0.0f;
    float y = 0.0f;
    py_ObjectRef tuple;
    (void)argc;
    (void)argv;
    if (g_host.mouse_position != NULL)
        g_host.mouse_position(g_host.userdata, &x, &y);
    tuple = py_newtuple(py_retval(), 2);
    py_newfloat(py_tuple_getitem(py_retval(), 0), x);
    py_newfloat(py_tuple_getitem(py_retval(), 1), y);
    (void)tuple;
    return true;
}

static bool py_screen_size(int argc, py_StackRef argv)
{
    int w = GR_FB_W;
    int h = GR_FB_H;
    py_ObjectRef tuple;
    (void)argc;
    (void)argv;
    if (g_host.framebuffer_size != NULL)
        g_host.framebuffer_size(g_host.userdata, &w, &h);
    tuple = py_newtuple(py_retval(), 2);
    py_newint(py_tuple_getitem(py_retval(), 0), w);
    py_newint(py_tuple_getitem(py_retval(), 1), h);
    (void)tuple;
    return true;
}

static bool py_set_title(int argc, py_StackRef argv)
{
    const char *title;
    (void)argc;
    if (!py_isstr(py_arg(0)))
        return TypeError("title must be a string");
    title = py_tostr(py_arg(0));
    if (g_host.set_title != NULL)
        g_host.set_title(g_host.userdata, title);
    py_newnone(py_retval());
    return true;
}

static bool py_quit(int argc, py_StackRef argv)
{
    (void)argc;
    (void)argv;
    if (g_host.request_quit != NULL)
        g_host.request_quit(g_host.userdata);
    py_newnone(py_retval());
    return true;
}

static bool py_viewport(int argc, py_StackRef argv)
{
    int x, y, w, h;
    (void)argc;
    if (!cast_int_arg(0, argv, &x) || !cast_int_arg(1, argv, &y)
        || !cast_int_arg(2, argv, &w) || !cast_int_arg(3, argv, &h))
        return false;
    grViewport(x, y, w, h);
    py_newnone(py_retval());
    return true;
}

static bool py_clip_window(int argc, py_StackRef argv)
{
    int xmin, ymin, xmax, ymax;
    (void)argc;
    if (!cast_int_arg(0, argv, &xmin) || !cast_int_arg(1, argv, &ymin)
        || !cast_int_arg(2, argv, &xmax) || !cast_int_arg(3, argv, &ymax))
        return false;
    grClipWindow(xmin, ymin, xmax, ymax);
    py_newnone(py_retval());
    return true;
}

static bool py_color_combine(int argc, py_StackRef argv)
{
    int func, factor, local, other;
    bool invert;
    (void)argc;
    if (!cast_int_arg(0, argv, &func) || !cast_int_arg(1, argv, &factor)
        || !cast_int_arg(2, argv, &local) || !cast_int_arg(3, argv, &other))
        return false;
    invert = py_tobool(py_arg(4));
    grColorCombine((GrCombineFunction_t)func,
                   (GrCombineFactor_t)factor,
                   (GrCombineLocal_t)local,
                   (GrCombineOther_t)other,
                   invert);
    py_newnone(py_retval());
    return true;
}

static bool py_alpha_combine(int argc, py_StackRef argv)
{
    int func, factor, local, other;
    bool invert;
    (void)argc;
    if (!cast_int_arg(0, argv, &func) || !cast_int_arg(1, argv, &factor)
        || !cast_int_arg(2, argv, &local) || !cast_int_arg(3, argv, &other))
        return false;
    invert = py_tobool(py_arg(4));
    grAlphaCombine((GrCombineFunction_t)func,
                   (GrCombineFactor_t)factor,
                   (GrCombineLocal_t)local,
                   (GrCombineOther_t)other,
                   invert);
    py_newnone(py_retval());
    return true;
}

static bool py_alpha_blend_function(int argc, py_StackRef argv)
{
    int rgb_sf, rgb_df, alpha_sf, alpha_df;
    (void)argc;
    if (!cast_int_arg(0, argv, &rgb_sf) || !cast_int_arg(1, argv, &rgb_df)
        || !cast_int_arg(2, argv, &alpha_sf) || !cast_int_arg(3, argv, &alpha_df))
        return false;
    grAlphaBlendFunction((GrAlphaBlendFnc_t)rgb_sf,
                         (GrAlphaBlendFnc_t)rgb_df,
                         (GrAlphaBlendFnc_t)alpha_sf,
                         (GrAlphaBlendFnc_t)alpha_df);
    py_newnone(py_retval());
    return true;
}

static bool py_alpha_test_function(int argc, py_StackRef argv)
{
    int func;
    (void)argc;
    if (!cast_int_arg(0, argv, &func))
        return false;
    grAlphaTestFunction((GrCmpFnc_t)func);
    py_newnone(py_retval());
    return true;
}

static bool py_alpha_test_ref(int argc, py_StackRef argv)
{
    int value;
    (void)argc;
    if (!cast_int_arg(0, argv, &value))
        return false;
    grAlphaTestReferenceValue((GrAlpha_t)value);
    py_newnone(py_retval());
    return true;
}

static bool py_constant_color(int argc, py_StackRef argv)
{
    int color;
    (void)argc;
    if (!cast_int_arg(0, argv, &color))
        return false;
    grConstantColorValue((GrColor_t)color);
    py_newnone(py_retval());
    return true;
}

static bool py_depth_buffer_mode(int argc, py_StackRef argv)
{
    int mode;
    (void)argc;
    if (!cast_int_arg(0, argv, &mode))
        return false;
    grDepthBufferMode((GrDepthBufferMode_t)mode);
    py_newnone(py_retval());
    return true;
}

static bool py_depth_buffer_function(int argc, py_StackRef argv)
{
    int func;
    (void)argc;
    if (!cast_int_arg(0, argv, &func))
        return false;
    grDepthBufferFunction((GrCmpFnc_t)func);
    py_newnone(py_retval());
    return true;
}

static bool py_depth_mask(int argc, py_StackRef argv)
{
    (void)argc;
    grDepthMask(py_tobool(py_arg(0)));
    py_newnone(py_retval());
    return true;
}

static bool py_cull_mode(int argc, py_StackRef argv)
{
    int mode;
    (void)argc;
    if (!cast_int_arg(0, argv, &mode))
        return false;
    grCullMode((GrCullMode_t)mode);
    py_newnone(py_retval());
    return true;
}

static bool py_shade_model(int argc, py_StackRef argv)
{
    int mode;
    (void)argc;
    if (!cast_int_arg(0, argv, &mode))
        return false;
    grShadeModel((GrShadeModel_t)mode);
    py_newnone(py_retval());
    return true;
}

static bool py_fog_mode(int argc, py_StackRef argv)
{
    int mode;
    (void)argc;
    if (!cast_int_arg(0, argv, &mode))
        return false;
    grFogMode((GrFogMode_t)mode);
    py_newnone(py_retval());
    return true;
}

static bool py_fog_color_value(int argc, py_StackRef argv)
{
    int color;
    (void)argc;
    if (!cast_int_arg(0, argv, &color))
        return false;
    grFogColorValue((GrColor_t)color);
    py_newnone(py_retval());
    return true;
}

static bool py_fog_table(int argc, py_StackRef argv)
{
    GrFog_t table[GR_FOG_TABLE_SIZE];
    int len;
    int i;
    (void)argc;
    if (seq_len(py_arg(0)) < 0)
        return TypeError("fog table must be a list or tuple");
    len = seq_len(py_arg(0));
    if (len != GR_FOG_TABLE_SIZE)
        return ValueError("fog table must contain %d entries", GR_FOG_TABLE_SIZE);
    for (i = 0; i < GR_FOG_TABLE_SIZE; i++) {
        py_i64 value = 0;
        if (!py_castint(seq_getitem(py_arg(0), i), &value))
            return TypeError("fog table entry %d must be an integer", i);
        if (value < 0 || value > 255)
            return ValueError("fog table entry %d must be in range 0..255", i);
        table[i] = (GrFog_t)value;
    }
    grFogTable(table);
    py_newnone(py_retval());
    return true;
}

static bool py_tex_allocate(int argc, py_StackRef argv)
{
    (void)argc;
    (void)argv;
    py_newint(py_retval(), grTexAllocate());
    return true;
}

static bool py_tex_free(int argc, py_StackRef argv)
{
    int tex;
    (void)argc;
    if (!cast_int_arg(0, argv, &tex))
        return false;
    grTexFree(tex);
    py_newnone(py_retval());
    return true;
}

static bool py_tex_download_mipmap(int argc, py_StackRef argv)
{
    int tex, w, h, fmt;
    int count;
    void *pixels;
    (void)argc;
    if (!cast_int_arg(0, argv, &tex) || !cast_int_arg(1, argv, &w)
        || !cast_int_arg(2, argv, &h) || !cast_int_arg(3, argv, &fmt))
        return false;
    if (w <= 0 || h <= 0)
        return ValueError("texture width and height must be positive");
    count = w * h;
    if ((GrTextureFormat)fmt == GR_TEXFMT_RGB_565 || (GrTextureFormat)fmt == GR_TEXFMT_ARGB_1555) {
        uint16_t *tmp = (uint16_t *)malloc((size_t)count * sizeof(uint16_t));
        if (tmp == NULL)
            return RuntimeError("out of memory while decoding texture upload");
        if (!parse_u16_list(py_arg(4), tmp, count)) {
            free(tmp);
            return false;
        }
        pixels = tmp;
    } else if ((GrTextureFormat)fmt == GR_TEXFMT_ARGB_8888) {
        uint8_t *tmp = (uint8_t *)malloc((size_t)count * 4u);
        if (tmp == NULL)
            return RuntimeError("out of memory while decoding texture upload");
        if (!parse_byte_list(py_arg(4), tmp, count * 4)) {
            free(tmp);
            return false;
        }
        pixels = tmp;
    } else {
        return ValueError("unsupported texture format: %d", fmt);
    }
    grTexDownloadMipMap(tex, pixels, w, h, (GrTextureFormat)fmt);
    free(pixels);
    py_newnone(py_retval());
    return true;
}

static bool py_tex_bind(int argc, py_StackRef argv)
{
    int tex;
    (void)argc;
    if (!cast_int_arg(0, argv, &tex))
        return false;
    grTexBind(tex);
    py_newnone(py_retval());
    return true;
}

static bool py_tex_filter(int argc, py_StackRef argv)
{
    int tex, mm, minf, magf;
    (void)argc;
    if (!cast_int_arg(0, argv, &tex) || !cast_int_arg(1, argv, &mm)
        || !cast_int_arg(2, argv, &minf) || !cast_int_arg(3, argv, &magf))
        return false;
    grTexFilter(tex, (GrMipMapMode)mm, (GrTextureFilter)minf, (GrTextureFilter)magf);
    py_newnone(py_retval());
    return true;
}

static bool py_tex_clamp_mode(int argc, py_StackRef argv)
{
    int tex, s_clamp, t_clamp;
    (void)argc;
    if (!cast_int_arg(0, argv, &tex) || !cast_int_arg(1, argv, &s_clamp)
        || !cast_int_arg(2, argv, &t_clamp))
        return false;
    grTexClampMode(tex, (GrTextureClampMode)s_clamp, (GrTextureClampMode)t_clamp);
    py_newnone(py_retval());
    return true;
}

static bool py_draw_triangle(int argc, py_StackRef argv)
{
    GrVertex v0, v1, v2;
    (void)argc;
    if (!parse_vertex(py_arg(0), &v0) || !parse_vertex(py_arg(1), &v1) || !parse_vertex(py_arg(2), &v2))
        return false;
    grDrawTriangle(&v0, &v1, &v2);
    py_newnone(py_retval());
    return true;
}

static bool py_draw_point(int argc, py_StackRef argv)
{
    GrVertex v;
    (void)argc;
    if (!parse_vertex(py_arg(0), &v))
        return false;
    grDrawPoint(&v);
    py_newnone(py_retval());
    return true;
}

static bool py_draw_line(int argc, py_StackRef argv)
{
    GrVertex v0, v1;
    (void)argc;
    if (!parse_vertex(py_arg(0), &v0) || !parse_vertex(py_arg(1), &v1))
        return false;
    grDrawLine(&v0, &v1);
    py_newnone(py_retval());
    return true;
}

bool glBindingsRegister(py_Ref module)
{
    py_bind(module, "color_pack(r, g, b, a)", py_color_pack);
    py_bind(module, "color_unpack(color)", py_color_unpack);
    py_bind(module, "init_renderer(width, height)", py_init_renderer);
    py_bind(module, "shutdown_renderer()", py_shutdown_renderer);
    py_bind(module, "buffer_clear(color, alpha, depth)", py_buffer_clear);
    py_bind(module, "buffer_swap_current()", py_buffer_swap_current);
    py_bind(module, "time()", py_time_now);
    py_bind(module, "dt()", py_delta_time);
    py_bind(module, "key_down(key)", py_key_down);
    py_bind(module, "key_pressed(key)", py_key_pressed);
    py_bind(module, "mouse_down(button)", py_mouse_down);
    py_bind(module, "mouse_position()", py_mouse_position);
    py_bind(module, "screen_size()", py_screen_size);
    py_bind(module, "set_title(title)", py_set_title);
    py_bind(module, "quit()", py_quit);
    py_bind(module, "viewport(x, y, width, height)", py_viewport);
    py_bind(module, "clip_window(xmin, ymin, xmax, ymax)", py_clip_window);
    py_bind(module, "color_combine(func, factor, local, other, invert)", py_color_combine);
    py_bind(module, "alpha_combine(func, factor, local, other, invert)", py_alpha_combine);
    py_bind(module, "alpha_blend_function(rgb_sf, rgb_df, alpha_sf, alpha_df)", py_alpha_blend_function);
    py_bind(module, "alpha_test_function(func)", py_alpha_test_function);
    py_bind(module, "alpha_test_reference_value(value)", py_alpha_test_ref);
    py_bind(module, "constant_color_value(color)", py_constant_color);
    py_bind(module, "depth_buffer_mode(mode)", py_depth_buffer_mode);
    py_bind(module, "depth_buffer_function(func)", py_depth_buffer_function);
    py_bind(module, "depth_mask(enabled)", py_depth_mask);
    py_bind(module, "cull_mode(mode)", py_cull_mode);
    py_bind(module, "shade_model(mode)", py_shade_model);
    py_bind(module, "fog_mode(mode)", py_fog_mode);
    py_bind(module, "fog_color_value(color)", py_fog_color_value);
    py_bind(module, "fog_table(values)", py_fog_table);
    py_bind(module, "tex_allocate()", py_tex_allocate);
    py_bind(module, "tex_free(tex)", py_tex_free);
    py_bind(module, "tex_download_mipmap(tex, width, height, fmt, pixels)", py_tex_download_mipmap);
    py_bind(module, "tex_bind(tex)", py_tex_bind);
    py_bind(module, "tex_filter(tex, mipmap_mode, min_filter, mag_filter)", py_tex_filter);
    py_bind(module, "tex_clamp_mode(tex, s_clamp, t_clamp)", py_tex_clamp_mode);
    py_bind(module, "draw_triangle(v0, v1, v2)", py_draw_triangle);
    py_bind(module, "draw_point(v)", py_draw_point);
    py_bind(module, "draw_line(v0, v1)", py_draw_line);

    set_module_int(module, "FB_W", GR_FB_W);
    set_module_int(module, "FB_H", GR_FB_H);
    set_module_int(module, "MAX_TEXTURES", GR_MAX_TEXTURES);
    set_module_int(module, "MAX_TEX_SIZE", GR_MAX_TEX_SIZE);
    set_module_int(module, "FOG_TABLE_SIZE", GR_FOG_TABLE_SIZE);

    set_module_int(module, "CMP_NEVER", GR_CMP_NEVER);
    set_module_int(module, "CMP_LESS", GR_CMP_LESS);
    set_module_int(module, "CMP_EQUAL", GR_CMP_EQUAL);
    set_module_int(module, "CMP_LEQUAL", GR_CMP_LEQUAL);
    set_module_int(module, "CMP_GREATER", GR_CMP_GREATER);
    set_module_int(module, "CMP_NOTEQUAL", GR_CMP_NOTEQUAL);
    set_module_int(module, "CMP_GEQUAL", GR_CMP_GEQUAL);
    set_module_int(module, "CMP_ALWAYS", GR_CMP_ALWAYS);

    set_module_int(module, "BLEND_ZERO", GR_BLEND_ZERO);
    set_module_int(module, "BLEND_ONE", GR_BLEND_ONE);
    set_module_int(module, "BLEND_SRC_COLOR", GR_BLEND_SRC_COLOR);
    set_module_int(module, "BLEND_ONE_MINUS_SRC_COLOR", GR_BLEND_ONE_MINUS_SRC_COLOR);
    set_module_int(module, "BLEND_DST_COLOR", GR_BLEND_DST_COLOR);
    set_module_int(module, "BLEND_ONE_MINUS_DST_COLOR", GR_BLEND_ONE_MINUS_DST_COLOR);
    set_module_int(module, "BLEND_SRC_ALPHA", GR_BLEND_SRC_ALPHA);
    set_module_int(module, "BLEND_ONE_MINUS_SRC_ALPHA", GR_BLEND_ONE_MINUS_SRC_ALPHA);
    set_module_int(module, "BLEND_DST_ALPHA", GR_BLEND_DST_ALPHA);
    set_module_int(module, "BLEND_ONE_MINUS_DST_ALPHA", GR_BLEND_ONE_MINUS_DST_ALPHA);
    set_module_int(module, "BLEND_ALPHA_SATURATE", GR_BLEND_ALPHA_SATURATE);

    set_module_int(module, "COMBINE_FUNCTION_ZERO", GR_COMBINE_FUNCTION_ZERO);
    set_module_int(module, "COMBINE_FUNCTION_LOCAL", GR_COMBINE_FUNCTION_LOCAL);
    set_module_int(module, "COMBINE_FUNCTION_LOCAL_ALPHA", GR_COMBINE_FUNCTION_LOCAL_ALPHA);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER", GR_COMBINE_FUNCTION_SCALE_OTHER);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL", GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA", GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL", GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL", GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA", GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL", GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL);
    set_module_int(module, "COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA", GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA);

    set_module_int(module, "COMBINE_FACTOR_NONE", GR_COMBINE_FACTOR_NONE);
    set_module_int(module, "COMBINE_FACTOR_ZERO", GR_COMBINE_FACTOR_ZERO);
    set_module_int(module, "COMBINE_FACTOR_LOCAL", GR_COMBINE_FACTOR_LOCAL);
    set_module_int(module, "COMBINE_FACTOR_OTHER_ALPHA", GR_COMBINE_FACTOR_OTHER_ALPHA);
    set_module_int(module, "COMBINE_FACTOR_LOCAL_ALPHA", GR_COMBINE_FACTOR_LOCAL_ALPHA);
    set_module_int(module, "COMBINE_FACTOR_TEXTURE_ALPHA", GR_COMBINE_FACTOR_TEXTURE_ALPHA);
    set_module_int(module, "COMBINE_FACTOR_ONE", GR_COMBINE_FACTOR_ONE);
    set_module_int(module, "COMBINE_FACTOR_ONE_MINUS_LOCAL", GR_COMBINE_FACTOR_ONE_MINUS_LOCAL);
    set_module_int(module, "COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA", GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA);
    set_module_int(module, "COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA", GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA);
    set_module_int(module, "COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA", GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA);

    set_module_int(module, "COMBINE_LOCAL_NONE", GR_COMBINE_LOCAL_NONE);
    set_module_int(module, "COMBINE_LOCAL_ITERATED", GR_COMBINE_LOCAL_ITERATED);
    set_module_int(module, "COMBINE_LOCAL_CONSTANT", GR_COMBINE_LOCAL_CONSTANT);
    set_module_int(module, "COMBINE_LOCAL_DEPTH", GR_COMBINE_LOCAL_DEPTH);

    set_module_int(module, "COMBINE_OTHER_NONE", GR_COMBINE_OTHER_NONE);
    set_module_int(module, "COMBINE_OTHER_ITERATED", GR_COMBINE_OTHER_ITERATED);
    set_module_int(module, "COMBINE_OTHER_TEXTURE", GR_COMBINE_OTHER_TEXTURE);
    set_module_int(module, "COMBINE_OTHER_CONSTANT", GR_COMBINE_OTHER_CONSTANT);

    set_module_int(module, "FOG_DISABLE", GR_FOG_DISABLE);
    set_module_int(module, "FOG_WITH_ITERATED_ALPHA", GR_FOG_WITH_ITERATED_ALPHA);
    set_module_int(module, "FOG_WITH_TABLE", GR_FOG_WITH_TABLE);

    set_module_int(module, "CULL_DISABLE", GR_CULL_DISABLE);
    set_module_int(module, "CULL_NEGATIVE", GR_CULL_NEGATIVE);
    set_module_int(module, "CULL_POSITIVE", GR_CULL_POSITIVE);

    set_module_int(module, "DEPTHBUFFER_DISABLE", GR_DEPTHBUFFER_DISABLE);
    set_module_int(module, "DEPTHBUFFER_ZBUFFER", GR_DEPTHBUFFER_ZBUFFER);

    set_module_int(module, "SHADE_FLAT", GR_SHADE_FLAT);
    set_module_int(module, "SHADE_GOURAUD", GR_SHADE_GOURAUD);
    set_module_int(module, "SHADE_COLOR", GR_SHADE_COLOR);
    set_module_int(module, "SHADE_ALPHA", GR_SHADE_ALPHA);
    set_module_int(module, "SHADE_ST", GR_SHADE_ST);
    set_module_int(module, "SHADE_Z", GR_SHADE_Z);
    set_module_int(module, "SHADE_W", GR_SHADE_W);

    set_module_int(module, "TEXFMT_RGB_565", GR_TEXFMT_RGB_565);
    set_module_int(module, "TEXFMT_ARGB_1555", GR_TEXFMT_ARGB_1555);
    set_module_int(module, "TEXFMT_ARGB_8888", GR_TEXFMT_ARGB_8888);

    set_module_int(module, "MIPMAP_DISABLE", GR_MIPMAP_DISABLE);
    set_module_int(module, "MIPMAP_NEAREST", GR_MIPMAP_NEAREST);
    set_module_int(module, "MIPMAP_NEAREST_DITHER", GR_MIPMAP_NEAREST_DITHER);

    set_module_int(module, "TEXTUREFILTER_POINT_SAMPLED", GR_TEXTUREFILTER_POINT_SAMPLED);
    set_module_int(module, "TEXTUREFILTER_BILINEAR", GR_TEXTUREFILTER_BILINEAR);

    set_module_int(module, "TEXTURECLAMP_WRAP", GR_TEXTURECLAMP_WRAP);
    set_module_int(module, "TEXTURECLAMP_CLAMP", GR_TEXTURECLAMP_CLAMP);

    if (!exec_bootstrap(module))
        return false;
    return true;
}
