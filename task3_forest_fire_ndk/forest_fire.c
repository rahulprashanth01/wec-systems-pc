// Task III - Forest fire simulation using OpenGL ES 3.1 compute shaders

#include <EGL/egl.h>
#include <GLES3/gl31.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// cell states
#define NOTHING  0
#define HEALTHY  1
#define BURNING  2

/* ----- compute shader source ----- */
static const char *compute_src =
    "#version 310 es\n"
    "layout(local_size_x = 16, local_size_y = 16) in;\n"
    "\n"
    "layout(std430, binding = 0) readonly  buffer InBuf  { uint src[]; };\n"
    "layout(std430, binding = 1) writeonly buffer OutBuf { uint dst[]; };\n"
    "layout(std430, binding = 2)           buffer Ctr    { coherent uint num_burning; };\n"
    "\n"
    "uniform uint side;\n"
    "uniform uint cur_epoch;\n"
    "\n"
    "// simple integer hash for per-cell randomness\n"
    "uint hash(uint x) {\n"
    "    x ^= x >> 16u; x *= 0x7feb352du;\n"
    "    x ^= x >> 15u; x *= 0x846ca68bu;\n"
    "    return x ^ (x >> 16u);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    uvec2 pos = gl_GlobalInvocationID.xy;\n"
    "    if (pos.x >= side || pos.y >= side) return;\n"
    "\n"
    "    uint idx = pos.y * side + pos.x;\n"
    "    uint state = src[idx];\n"
    "    uint next = state;\n"
    "\n"
    "    // check 8-neighbours for fire\n"
    "    bool has_burning_neighbour = false;\n"
    "    for (int dy = -1; dy <= 1; dy++) {\n"
    "        for (int dx = -1; dx <= 1; dx++) {\n"
    "            if (dx == 0 && dy == 0) continue;\n"
    "            int nx = int(pos.x) + dx;\n"
    "            int ny = int(pos.y) + dy;\n"
    "            if (nx >= 0 && ny >= 0 && nx < int(side) && ny < int(side)) {\n"
    "                if (src[uint(ny) * side + uint(nx)] == 2u)\n"
    "                    has_burning_neighbour = true;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // burning -> nothing\n"
    "    if (state == 2u) {\n"
    "        next = 0u;\n"
    "    }\n"
    "    // healthy + neighbour on fire -> catch fire with p=0.15\n"
    "    else if (state == 1u && has_burning_neighbour) {\n"
    "        if (hash(idx ^ (cur_epoch * 0x9e3779b9u)) % 100u < 15u)\n"
    "            next = 2u;\n"
    "    }\n"
    "\n"
    "    dst[idx] = next;\n"
    "    if (next == 2u) atomicAdd(num_burning, 1u);\n"
    "}\n";

/* ----- helpers ----- */

static void bail(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(1);
}

static void check_egl(const char *where) {
    EGLint err = eglGetError();
    if (err != EGL_SUCCESS) {
        fprintf(stderr, "EGL error at %s: 0x%x\n", where, err);
        exit(1);
    }
}

static void check_gl(const char *where) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "GL error at %s: 0x%x\n", where, err);
        exit(1);
    }
}

/* set up an EGL context for headless compute */
static EGLDisplay setup_egl(EGLContext *ctx, EGLSurface *surf) {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) bail("eglGetDisplay failed");
    if (!eglInitialize(dpy, NULL, NULL)) check_egl("eglInitialize");

    EGLint cfg_attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(dpy, cfg_attrs, &config, 1, &num_configs) || num_configs == 0)
        bail("couldn't find a GLES3 EGL config");

    // try surfaceless extension first (cleaner, no dummy surface needed)
    const char *extensions = eglQueryString(dpy, EGL_EXTENSIONS);
    int have_surfaceless = extensions && strstr(extensions, "EGL_KHR_surfaceless_context");

    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    *ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attrs);
    if (*ctx == EGL_NO_CONTEXT) check_egl("eglCreateContext");

    *surf = EGL_NO_SURFACE;
    if (!have_surfaceless) {
        // fallback: create a tiny pbuffer just to make EGL happy
        EGLint pb_attrs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
        *surf = eglCreatePbufferSurface(dpy, config, pb_attrs);
        if (*surf == EGL_NO_SURFACE) check_egl("eglCreatePbufferSurface");
    }

    if (!eglMakeCurrent(dpy, *surf, *surf, *ctx))
        check_egl("eglMakeCurrent");

    printf("surfaceless context: %s\n", have_surfaceless ? "yes" : "no (using 1x1 pbuffer)");
    return dpy;
}

/* compile the compute shader and link it into a program */
static GLuint build_shader_program(void) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &compute_src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile error:\n%s\n", log);
        exit(1);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glDeleteShader(shader);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "program link error:\n%s\n", log);
        exit(1);
    }
    return prog;
}

/* print the grid to terminal (only for small grids) */
static void dump_grid(const uint32_t *grid, unsigned m) {
    for (unsigned y = 0; y < m; y++) {
        for (unsigned x = 0; x < m; x++) {
            uint32_t cell = grid[y * m + x];
            putchar(cell == HEALTHY ? 'H' : cell == BURNING ? 'B' : '.');
        }
        putchar('\n');
    }
}

/* run one simulation for a given grid size */
static void run_simulation(unsigned m) {
    size_t grid_bytes = (size_t)m * m * sizeof(uint32_t);

    // set up initial grid on CPU
    uint32_t *grid = malloc(grid_bytes);
    if (!grid) bail("malloc failed");

    // ~70% of cells start as healthy trees (deterministic pattern)
    for (unsigned i = 0; i < m * m; i++)
        grid[i] = ((i * 1103515245u + 12345u) >> 28u) < 11u ? HEALTHY : NOTHING;

    // light the center on fire
    grid[(m / 2) * m + (m / 2)] = BURNING;

    // create GPU buffers: two grid SSBOs (ping-pong) + one counter SSBO
    GLuint bufs[3];
    glGenBuffers(3, bufs);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, grid_bytes, grid, GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, grid_bytes, NULL, GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[2]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), NULL, GL_DYNAMIC_COPY);

    GLuint prog = build_shader_program();
    glUseProgram(prog);
    GLint u_side  = glGetUniformLocation(prog, "side");
    GLint u_epoch = glGetUniformLocation(prog, "cur_epoch");

    unsigned epoch = 0;
    unsigned cur = 0;  // which buffer is "current" (0 or 1)
    unsigned still_burning = 1;

    if (m <= 20) {
        printf("--- epoch 0 ---\n");
        dump_grid(grid, m);
    }

    while (still_burning) {
        // zero the burning counter
        uint32_t zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[2]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), &zero);

        // bind input/output/counter
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[cur]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1 - cur]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufs[2]);

        glUniform1ui(u_side, m);
        glUniform1ui(u_epoch, epoch);
        glDispatchCompute((m + 15) / 16, (m + 15) / 16, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        epoch++;

        // read back the burning count
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[2]);
        uint32_t *ctr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(*ctr), GL_MAP_READ_BIT);
        if (!ctr) bail("failed to map counter buffer");
        still_burning = *ctr;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        cur = 1 - cur;  // swap buffers

        // print grid for small sizes
        if (m <= 20) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[cur]);
            uint32_t *out = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, grid_bytes, GL_MAP_READ_BIT);
            if (!out) bail("failed to map grid buffer");
            printf("--- epoch %u (burning: %u) ---\n", epoch, still_burning);
            dump_grid(out, m);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        check_gl("simulation step");
    }

    printf("M=%u: fire went out after %u epochs\n\n", m, epoch);

    glDeleteProgram(prog);
    glDeleteBuffers(3, bufs);
    free(grid);
}

int main(int argc, char *argv[]) {
    EGLContext ctx;
    EGLSurface surf;
    EGLDisplay dpy = setup_egl(&ctx, &surf);

    printf("GL version: %s\n\n", (const char *)glGetString(GL_VERSION));

    if (argc == 1) {
        // default test sizes
        unsigned sizes[] = {10, 20, 128};
        for (int i = 0; i < 3; i++)
            run_simulation(sizes[i]);
    } else {
        for (int i = 1; i < argc; i++) {
            unsigned m = (unsigned)strtoul(argv[i], NULL, 10);
            if (m < 1 || m > 16384) {
                fprintf(stderr, "M=%u out of range (1..16384)\n", m);
                continue;
            }
            run_simulation(m);
        }
    }

    // cleanup
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
