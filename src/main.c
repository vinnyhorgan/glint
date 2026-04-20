#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gles2.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>

typedef struct Demo
{
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint quad_buffer;
    GLint position_attrib;
    GLint resolution_uniform;
    GLint time_uniform;
} Demo;

static const GLchar* const kVertexShaderSource[] = {
    "attribute vec2 a_pos;\n",
    "varying vec2 v_uv;\n",
    "\n",
    "void main(void)\n",
    "{\n",
    "    v_uv = 0.5 * (a_pos + 1.0);\n",
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n",
    "}\n",
};

static const GLchar* const kFragmentShaderSource[] = {
    "precision mediump float;\n",
    "\n",
    "varying vec2 v_uv;\n",
    "uniform vec2 u_resolution;\n",
    "uniform float u_time;\n",
    "\n",
    "float box(vec2 p, vec2 min_corner, vec2 max_corner)\n",
    "{\n",
    "    vec2 inside = step(min_corner, p) * step(p, max_corner);\n",
    "    return inside.x * inside.y;\n",
    "}\n",
    "\n",
    "float glyph_h(vec2 p)\n",
    "{\n",
    "    return max(max(box(p, vec2(0.06, 0.08), vec2(0.20, 0.92)),\n",
    "                   box(p, vec2(0.80, 0.08), vec2(0.94, 0.92))),\n",
    "               box(p, vec2(0.20, 0.44), vec2(0.80, 0.58)));\n",
    "}\n",
    "\n",
    "float glyph_e(vec2 p)\n",
    "{\n",
    "    return max(max(box(p, vec2(0.08, 0.08), vec2(0.22, 0.92)),\n",
    "                   box(p, vec2(0.22, 0.78), vec2(0.92, 0.92))),\n",
    "               max(box(p, vec2(0.22, 0.44), vec2(0.76, 0.58)),\n",
    "                   box(p, vec2(0.22, 0.08), vec2(0.92, 0.22))));\n",
    "}\n",
    "\n",
    "float glyph_l(vec2 p)\n",
    "{\n",
    "    return max(box(p, vec2(0.08, 0.08), vec2(0.22, 0.92)),\n",
    "               box(p, vec2(0.22, 0.08), vec2(0.90, 0.22)));\n",
    "}\n",
    "\n",
    "float glyph_o(vec2 p)\n",
    "{\n",
    "    float outer = box(p, vec2(0.08, 0.08), vec2(0.92, 0.92));\n",
    "    float inner = box(p, vec2(0.26, 0.24), vec2(0.74, 0.76));\n",
    "    return max(0.0, outer - inner);\n",
    "}\n",
    "\n",
    "float glyph_w(vec2 p)\n",
    "{\n",
    "    return max(max(box(p, vec2(0.04, 0.08), vec2(0.18, 0.92)),\n",
    "                   box(p, vec2(0.82, 0.08), vec2(0.96, 0.92))),\n",
    "               max(box(p, vec2(0.30, 0.08), vec2(0.44, 0.52)),\n",
    "                   box(p, vec2(0.56, 0.08), vec2(0.70, 0.52))));\n",
    "}\n",
    "\n",
    "float glyph_r(vec2 p)\n",
    "{\n",
    "    float stem = box(p, vec2(0.08, 0.08), vec2(0.22, 0.92));\n",
    "    float top = box(p, vec2(0.22, 0.70), vec2(0.88, 0.92));\n",
    "    float mid = box(p, vec2(0.22, 0.44), vec2(0.82, 0.58));\n",
    "    float right = box(p, vec2(0.74, 0.58), vec2(0.88, 0.92));\n",
    "    float leg = box(p, vec2(0.54, 0.08), vec2(0.70, 0.44));\n",
    "    return max(max(stem, top), max(max(mid, right), leg));\n",
    "}\n",
    "\n",
    "float glyph_d(vec2 p)\n",
    "{\n",
    "    float stem = box(p, vec2(0.08, 0.08), vec2(0.22, 0.92));\n",
    "    float body = box(p, vec2(0.22, 0.08), vec2(0.88, 0.92));\n",
    "    float cut = box(p, vec2(0.36, 0.24), vec2(0.70, 0.76));\n",
    "    return max(stem, max(0.0, body - cut));\n",
    "}\n",
    "\n",
    "float glyph_for_index(float index, vec2 p)\n",
    "{\n",
    "    if (index < 0.5)\n",
    "        return glyph_h(p);\n",
    "    if (index < 1.5)\n",
    "        return glyph_e(p);\n",
    "    if (index < 2.5)\n",
    "        return glyph_l(p);\n",
    "    if (index < 3.5)\n",
    "        return glyph_l(p);\n",
    "    if (index < 4.5)\n",
    "        return glyph_o(p);\n",
    "    if (index < 5.5)\n",
    "        return glyph_w(p);\n",
    "    if (index < 6.5)\n",
    "        return glyph_o(p);\n",
    "    if (index < 7.5)\n",
    "        return glyph_r(p);\n",
    "    if (index < 8.5)\n",
    "        return glyph_l(p);\n",
    "    return glyph_d(p);\n",
    "}\n",
    "\n",
    "float hello_world(vec2 uv)\n",
    "{\n",
    "    const float count = 10.0;\n",
    "    vec2 text_uv = (uv - vec2(0.12, 0.36)) / vec2(0.76, 0.18);\n",
    "\n",
    "    if (text_uv.x < 0.0 || text_uv.x > 1.0 || text_uv.y < 0.0 || text_uv.y > 1.0)\n",
    "        return 0.0;\n",
    "\n",
    "    float slot = text_uv.x * count;\n",
    "    float index = floor(slot);\n",
    "    vec2 glyph_uv = vec2(fract(slot), text_uv.y);\n",
    "\n",
    "    if (index > 4.5)\n",
    "        glyph_uv.x = (glyph_uv.x - 0.08) / 0.92;\n",
    "\n",
    "    if (glyph_uv.x < 0.0 || glyph_uv.x > 1.0)\n",
    "        return 0.0;\n",
    "\n",
    "    return glyph_for_index(index, glyph_uv);\n",
    "}\n",
    "\n",
    "void main(void)\n",
    "{\n",
    "    vec2 frag = gl_FragCoord.xy;\n",
    "    vec2 uv = frag / u_resolution;\n",
    "    vec2 p = (frag * 2.0 - u_resolution) / min(u_resolution.x, u_resolution.y);\n",
    "    float time = u_time;\n",
    "\n",
    "    float radial = length(p);\n",
    "    float sweep = 0.5 + 0.5 * sin(6.0 * radial - time * 2.8);\n",
    "    float bloom = exp(-3.4 * radial);\n",
    "    float grid = 0.5 + 0.5 * cos((p.x + p.y * 0.7) * 18.0 - time * 1.3);\n",
    "\n",
    "    vec3 base = vec3(0.02, 0.03, 0.06);\n",
    "    vec3 dawn = vec3(0.14, 0.32, 0.74) * (0.35 + 0.65 * uv.y);\n",
    "    vec3 neon = vec3(0.95, 0.38, 0.72) * sweep * bloom;\n",
    "    vec3 aqua = vec3(0.20, 0.92, 0.88) * grid * 0.12;\n",
    "    vec3 color = base + dawn + neon + aqua;\n",
    "\n",
    "    float halo = smoothstep(0.72, 0.08, radial);\n",
    "    color += vec3(0.7, 0.4, 1.0) * halo * 0.08;\n",
    "\n",
    "    float title = hello_world(uv);\n",
    "    float title_glow = smoothstep(0.0, 1.0, title) * (0.75 + 0.25 * sin(time * 2.0));\n",
    "    color = mix(color, vec3(1.0, 0.97, 0.92), title);\n",
    "    color += vec3(0.45, 0.85, 1.0) * title_glow * 0.24;\n",
    "\n",
    "    float vignette = smoothstep(1.3, 0.24, radial);\n",
    "    color *= vignette;\n",
    "\n",
    "    gl_FragColor = vec4(color, 1.0);\n",
    "}\n",
};

static const GLfloat kQuadVertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
    -1.0f,  1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
};

static void error_callback(int code, const char* description)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

static GLADapiproc glfw_glad_loader(const char* name)
{
    return glfwGetProcAddress(name);
}

static GLuint compile_shader(GLenum type, const GLchar* const* source, GLsizei source_count)
{
    GLint compiled = GL_FALSE;
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, source_count, source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint log_length = 0;
        char log[1024];

        glGetShaderInfoLog(shader, (GLsizei) sizeof(log), &log_length, log);
        fprintf(stderr, "Shader compile failed: %.*s\n", log_length, log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLint linked = GL_FALSE;
    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glBindAttribLocation(program, 0, "a_pos");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint log_length = 0;
        char log[1024];

        glGetProgramInfoLog(program, (GLsizei) sizeof(log), &log_length, log);
        fprintf(stderr, "Program link failed: %.*s\n", log_length, log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static int init_demo(Demo* demo)
{
    demo->vertex_shader = compile_shader(
        GL_VERTEX_SHADER,
        kVertexShaderSource,
        (GLsizei) (sizeof(kVertexShaderSource) / sizeof(kVertexShaderSource[0])));
    if (!demo->vertex_shader)
        return 0;

    demo->fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER,
        kFragmentShaderSource,
        (GLsizei) (sizeof(kFragmentShaderSource) / sizeof(kFragmentShaderSource[0])));
    if (!demo->fragment_shader)
        return 0;

    demo->program = link_program(demo->vertex_shader, demo->fragment_shader);
    if (!demo->program)
        return 0;

    demo->position_attrib = glGetAttribLocation(demo->program, "a_pos");
    demo->resolution_uniform = glGetUniformLocation(demo->program, "u_resolution");
    demo->time_uniform = glGetUniformLocation(demo->program, "u_time");

    glGenBuffers(1, &demo->quad_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, demo->quad_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return 1;
}

static void destroy_demo(Demo* demo)
{
    if (demo->quad_buffer)
        glDeleteBuffers(1, &demo->quad_buffer);
    if (demo->program)
        glDeleteProgram(demo->program);
    if (demo->fragment_shader)
        glDeleteShader(demo->fragment_shader);
    if (demo->vertex_shader)
        glDeleteShader(demo->vertex_shader);
}

static void render_demo(const Demo* demo, int width, int height, float time)
{
    glViewport(0, 0, width, height);
    glUseProgram(demo->program);
    glUniform2f(demo->resolution_uniform, (GLfloat) width, (GLfloat) height);
    glUniform1f(demo->time_uniform, time);

    glBindBuffer(GL_ARRAY_BUFFER, demo->quad_buffer);
    glEnableVertexAttribArray((GLuint) demo->position_attrib);
    glVertexAttribPointer((GLuint) demo->position_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray((GLuint) demo->position_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int main(void)
{
    Demo demo = {0};
    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(1120, 700, "glint hello world", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLES2(glfw_glad_loader))
    {
        fprintf(stderr, "Failed to load OpenGL ES 2.0 entry points via glad\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (!init_demo(&demo))
    {
        destroy_demo(&demo);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window))
    {
        int width;
        int height;

        glfwGetFramebufferSize(window, &width, &height);
        render_demo(&demo, width, height, (float) glfwGetTime());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    destroy_demo(&demo);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
