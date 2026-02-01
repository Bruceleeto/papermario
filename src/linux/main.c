// src/linux/main.c
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Types
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;

// GBI defines
#define G_VTX               0x01
#define G_TRI1              0x05
#define G_TRI2              0x06
#define G_QUAD              0x07
#define G_DL                0xDE
#define G_ENDDL             0xDF
#define G_MTX               0xDA
#define G_POPMTX            0xD8
#define G_TEXTURE           0xD7
#define G_SETTIMG           0xFD
#define G_LOADTLUT          0xF0
#define G_SETTILESIZE       0xF2
#define G_LOADBLOCK         0xF3
#define G_SETTILE           0xF5
#define G_SETPRIMCOLOR      0xFA
#define G_SETENVCOLOR       0xFB
#define G_SETFILLCOLOR      0xF7
#define G_FILLRECT          0xF6
#define G_MOVEWORD          0xDB
#define G_SETCOMBINE        0xFC
#define G_SETOTHERMODE_L    0xE3
#define G_SETOTHERMODE_H    0xE2
#define G_TEXRECT           0xE4
#define G_RDPHALF_1         0xF1
#define G_RDPHALF_2         0xE1
#define G_SETSCISSOR        0xED
#define G_RDPPIPESYNC       0xE7
#define G_RDPFULLSYNC       0xE9
#define G_RDPLOADSYNC       0xE6
#define G_RDPTILESYNC       0xE8
#define G_SETZIMG           0xFE
#define G_SETCIMG           0xFF
#define G_NOOP              0x00
#define G_GEOMETRYMODE      0xD9
#define G_MOVEMEM           0xDC

#define G_MTX_PROJECTION    0x04
#define G_MTX_LOAD          0x02
#define G_MW_SEGMENT        0x06

#define G_IM_FMT_RGBA       0
#define G_IM_FMT_CI         2
#define G_IM_FMT_IA         3
#define G_IM_FMT_I          4
#define G_IM_SIZ_4b         0
#define G_IM_SIZ_8b         1
#define G_IM_SIZ_16b        2
#define G_IM_SIZ_32b        3

// Structures
typedef struct {
    u32 w0, w1;
} GfxWords;

typedef union {
    GfxWords words;
    u64 force_align;
} Gfx;

typedef struct {
    s16 ob[3];
    u16 flag;
    s16 tc[2];
    u8 cn[4];
} Vtx_t;

typedef union {
    Vtx_t v;
} Vtx;

typedef struct {
    s32 m[4][4];
} Mtx;

// External - known symbol for address calculation
extern char D_80164000;
void boot_main(void* data);
void gfxRetrace_Callback(s32 gfxTaskNum);

// SDL/GL state
static SDL_Window* window;
static SDL_GLContext glctx;

// Address translation
static u32 sN64ToPC_Offset = 0;
static int sOffsetCalculated = 0;

// RSP state
static float sModelview[4][4];
static float sProjection[4][4];
static float sMVP[4][4];
static u16 sTexScaleS = 0xFFFF;
static u16 sTexScaleT = 0xFFFF;

// Vertex buffer
typedef struct {
    float x, y, z, w;
    float u, v;
    u8 r, g, b, a;
} Vertex;

static Vertex sVtxBuf[64];

// Colors
static float sPrimColor[4] = {1,1,1,1};
static float sEnvColor[4] = {1,1,1,1};
static u32 sFillColor = 0;

// Current texture
static u32 sCurTexAddr = 0;
static u32 sCurTexFmt = 0;
static u32 sCurTexSiz = 0;
static u32 sCurTexWidth = 0;
static GLuint sCurTexID = 0;

// TLUT
static u16 sTLUT[256];

// ============================================================================
// ADDRESS TRANSLATION
// ============================================================================
extern char __executable_start;  // Linker-provided symbol
extern char _end;                // End of BSS

void* pc_resolve_addr(u32 addr) {
    if (addr == 0) return NULL;
    return (void*)(uintptr_t)addr;
}
void gfx_swap_buffers(void) {
    SDL_GL_SwapWindow(window);
}

// ============================================================================
// HELPERS
// ============================================================================
static void swap64(u8* data, size_t size) {
    size_t i;
    u8 tmp;
    for (i = 0; i + 7 < size; i += 8) {
        tmp = data[i+0]; data[i+0] = data[i+7]; data[i+7] = tmp;
        tmp = data[i+1]; data[i+1] = data[i+6]; data[i+6] = tmp;
        tmp = data[i+2]; data[i+2] = data[i+5]; data[i+5] = tmp;
        tmp = data[i+3]; data[i+3] = data[i+4]; data[i+4] = tmp;
    }
}

static void mtx_identity(float m[4][4]) {
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

static void mtx_mul(float r[4][4], float a[4][4], float b[4][4]) {
    float t[4][4];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j] + a[i][3]*b[3][j];
    memcpy(r, t, sizeof(t));
}

/* Convert N64 fixed-point matrix to float */
static void mtx_l2f(float mf[4][4], Mtx* m) {
    int i, j;
    u32* src = (u32*)m->m;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            s32 int_part = (s16)(src[i*2 + j/2] >> (16 - (j&1)*16));
            u16 frac_part = (u16)(src[8 + i*2 + j/2] >> (16 - (j&1)*16));
            mf[i][j] = (float)int_part + (float)frac_part / 65536.0f;
        }
    }
}

// ============================================================================
// TEXTURE
// ============================================================================
static GLuint load_texture(u32 addr, u32 fmt, u32 siz, u32 w, u32 h) {
    void* src;
    u32* pixels;
    u8* swapped;
    size_t bytes;
    GLuint tex;
    u32 i;

    src = pc_resolve_addr(addr);
    if (!src || w == 0 || h == 0) return 0;

    pixels = (u32*)malloc(w * h * 4);
    if (!pixels) return 0;

    if (siz == G_IM_SIZ_4b) bytes = (w * h + 1) / 2;
    else if (siz == G_IM_SIZ_8b) bytes = w * h;
    else if (siz == G_IM_SIZ_16b) bytes = w * h * 2;
    else bytes = w * h * 4;

    swapped = (u8*)malloc(bytes);
    memcpy(swapped, src, bytes);
    swap64(swapped, bytes);

    if (fmt == G_IM_FMT_RGBA && siz == G_IM_SIZ_16b) {
        u16* s = (u16*)swapped;
        for (i = 0; i < w * h; i++) {
            u16 p = (s[i] >> 8) | (s[i] << 8);
            u8 r = ((p >> 11) & 0x1F) << 3;
            u8 g = ((p >> 6) & 0x1F) << 3;
            u8 b = ((p >> 1) & 0x1F) << 3;
            u8 a = (p & 1) ? 255 : 0;
            pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    } else if (fmt == G_IM_FMT_RGBA && siz == G_IM_SIZ_32b) {
        memcpy(pixels, swapped, bytes);
    } else if (fmt == G_IM_FMT_IA && siz == G_IM_SIZ_8b) {
        for (i = 0; i < w * h; i++) {
            u8 intensity = (swapped[i] >> 4) & 0xF;
            u8 alpha = swapped[i] & 0xF;
            intensity = (intensity << 4) | intensity;
            alpha = (alpha << 4) | alpha;
            pixels[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }
    } else if (fmt == G_IM_FMT_IA && siz == G_IM_SIZ_16b) {
        u16* s = (u16*)swapped;
        for (i = 0; i < w * h; i++) {
            u16 p = (s[i] >> 8) | (s[i] << 8);
            u8 intensity = p >> 8;
            u8 alpha = p & 0xFF;
            pixels[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }
    } else if (fmt == G_IM_FMT_I && siz == G_IM_SIZ_8b) {
        for (i = 0; i < w * h; i++) {
            u8 v = swapped[i];
            pixels[i] = (255 << 24) | (v << 16) | (v << 8) | v;
        }
    } else if (fmt == G_IM_FMT_CI && siz == G_IM_SIZ_8b) {
        for (i = 0; i < w * h; i++) {
            u16 tlut = sTLUT[swapped[i]];
            u8 r, g, b, a;
            tlut = (tlut >> 8) | (tlut << 8);
            r = ((tlut >> 11) & 0x1F) << 3;
            g = ((tlut >> 6) & 0x1F) << 3;
            b = ((tlut >> 1) & 0x1F) << 3;
            a = (tlut & 1) ? 255 : 0;
            pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    } else if (fmt == G_IM_FMT_CI && siz == G_IM_SIZ_4b) {
        for (i = 0; i < w * h; i++) {
            u8 idx = (i & 1) ? (swapped[i/2] & 0xF) : (swapped[i/2] >> 4);
            u16 tlut = sTLUT[idx];
            u8 r, g, b, a;
            tlut = (tlut >> 8) | (tlut << 8);
            r = ((tlut >> 11) & 0x1F) << 3;
            g = ((tlut >> 6) & 0x1F) << 3;
            b = ((tlut >> 1) & 0x1F) << 3;
            a = (tlut & 1) ? 255 : 0;
            pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    } else {
        /* Unknown format - magenta */
        for (i = 0; i < w * h; i++)
            pixels[i] = 0xFFFF00FF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    free(pixels);
    free(swapped);
    return tex;
}

// ============================================================================
// DISPLAY LIST
// ============================================================================
static void handle_vtx(u32 w0, u32 w1) {
    int n = (w0 >> 12) & 0xFF;
    int v0 = ((w0 >> 1) & 0x7F) - n;
    Vtx* src = (Vtx*)pc_resolve_addr(w1);
    float* m = (float*)sMVP;
    int i;

    if (!src || v0 < 0) return;

    for (i = 0; i < n && (v0 + i) < 64; i++) {
        Vtx_t* v = &src[i].v;
        Vertex* d = &sVtxBuf[v0 + i];
        float x = v->ob[0], y = v->ob[1], z = v->ob[2];
        s32 su, sv;

        d->x = m[0]*x + m[4]*y + m[8]*z + m[12];
        d->y = m[1]*x + m[5]*y + m[9]*z + m[13];
        d->z = m[2]*x + m[6]*y + m[10]*z + m[14];
        d->w = m[3]*x + m[7]*y + m[11]*z + m[15];

        su = ((s32)v->tc[0] * (s32)sTexScaleS) >> 16;
        sv = ((s32)v->tc[1] * (s32)sTexScaleT) >> 16;
        d->u = su / 32.0f;
        d->v = sv / 32.0f;

        d->r = v->cn[0];
        d->g = v->cn[1];
        d->b = v->cn[2];
        d->a = v->cn[3];
    }
}

static void draw_tri(int v0, int v1, int v2) {
    Vertex* verts[3];
    int i;

    if (v0 >= 64 || v1 >= 64 || v2 >= 64 || v0 < 0 || v1 < 0 || v2 < 0) return;

    verts[0] = &sVtxBuf[v0];
    verts[1] = &sVtxBuf[v1];
    verts[2] = &sVtxBuf[v2];

    glBegin(GL_TRIANGLES);
    for (i = 0; i < 3; i++) {
        Vertex* v = verts[i];
        float inv_w, sx, sy, sz;

        if (v->w < 0.001f && v->w > -0.001f) continue;

        inv_w = 1.0f / v->w;
        sx = (v->x * inv_w * 160.0f) + 160.0f;
        sy = 240.0f - ((v->y * inv_w * 120.0f) + 120.0f);
        sz = v->z * inv_w;

        glColor4ub(v->r, v->g, v->b, v->a);
        glTexCoord2f(v->u / 32.0f, v->v / 32.0f);
        glVertex3f(sx, sy, sz);
    }
    glEnd();
}

static void walk_dl(Gfx* dl, int depth) {
    int i;

    if (!dl || depth > 32) return;

    for (i = 0; i < 10000; i++) {
        u32 w0 = dl[i].words.w0;
        u32 w1 = dl[i].words.w1;
        u8 cmd = (w0 >> 24) & 0xFF;

        switch (cmd) {
            case G_NOOP:
            case G_RDPPIPESYNC:
            case G_RDPFULLSYNC:
            case G_RDPLOADSYNC:
            case G_RDPTILESYNC:
                /* Sync commands - ignore */
                break;

            case G_VTX:
                handle_vtx(w0, w1);
                break;

            case G_TRI1:
                draw_tri(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
                break;

            case G_TRI2:
                draw_tri(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
                draw_tri(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
                break;

            case G_QUAD:
                draw_tri(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
                draw_tri(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
                break;

            case G_MTX: {
                Mtx* mtx = (Mtx*)pc_resolve_addr(w1);
                float m[4][4];
                u8 params;
                if (!mtx) break;
                mtx_l2f(m, mtx);
                params = w0 & 0xFF;
                if (params & G_MTX_PROJECTION) {
                    if (params & G_MTX_LOAD) memcpy(sProjection, m, sizeof(m));
                    else mtx_mul(sProjection, m, sProjection);
                } else {
                    if (params & G_MTX_LOAD) memcpy(sModelview, m, sizeof(m));
                    else mtx_mul(sModelview, m, sModelview);
                }
                mtx_mul(sMVP, sModelview, sProjection);
            } break;

            case G_POPMTX:
                mtx_identity(sModelview);
                mtx_mul(sMVP, sModelview, sProjection);
                break;

            case G_DL: {
                void* target = pc_resolve_addr(w1);
                if (target) {
                    walk_dl((Gfx*)target, depth + 1);
                }
                /* If push flag is set, return after the call */
                if ((w0 >> 16) & 0xFF) return;
            } break;

            case G_ENDDL:
                return;

            case G_TEXTURE:
                sTexScaleS = (w1 >> 16) & 0xFFFF;
                sTexScaleT = w1 & 0xFFFF;
                break;

            case G_SETTIMG:
                sCurTexFmt = (w0 >> 21) & 0x7;
                sCurTexSiz = (w0 >> 19) & 0x3;
                sCurTexWidth = (w0 & 0xFFF) + 1;
                sCurTexAddr = w1;
                break;

            case G_LOADTLUT: {
                int count = ((w1 >> 14) & 0x3FF) + 1;
                void* data = pc_resolve_addr(sCurTexAddr);
                if (data && count <= 256) {
                    memcpy(sTLUT, data, count * 2);
                    swap64((u8*)sTLUT, count * 2);
                }
            } break;

            case G_SETTILESIZE: {
                int tile = (w1 >> 24) & 0x7;
                u32 lrs = (w1 >> 12) & 0xFFF;
                u32 lrt = w1 & 0xFFF;
                int w = (lrs >> 2) + 1;
                int h = (lrt >> 2) + 1;
                if (tile == 0 && sCurTexAddr) {
                    if (sCurTexID) glDeleteTextures(1, &sCurTexID);
                    sCurTexID = load_texture(sCurTexAddr, sCurTexFmt, sCurTexSiz, w, h);
                    if (sCurTexID) {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, sCurTexID);
                    }
                }
            } break;

            case G_SETPRIMCOLOR:
                sPrimColor[0] = ((w1 >> 24) & 0xFF) / 255.0f;
                sPrimColor[1] = ((w1 >> 16) & 0xFF) / 255.0f;
                sPrimColor[2] = ((w1 >> 8) & 0xFF) / 255.0f;
                sPrimColor[3] = (w1 & 0xFF) / 255.0f;
                break;

            case G_SETENVCOLOR:
                sEnvColor[0] = ((w1 >> 24) & 0xFF) / 255.0f;
                sEnvColor[1] = ((w1 >> 16) & 0xFF) / 255.0f;
                sEnvColor[2] = ((w1 >> 8) & 0xFF) / 255.0f;
                sEnvColor[3] = (w1 & 0xFF) / 255.0f;
                break;

            case G_SETFILLCOLOR:
                sFillColor = w1;
                break;

            case G_FILLRECT: {
                u32 xl = (w1 >> 14) & 0x3FF;
                u32 yl = (w1 >> 2) & 0x3FF;
                u32 xh = (w0 >> 14) & 0x3FF;
                u32 yh = (w0 >> 2) & 0x3FF;
                u16 c;
                float r, g, b;

                /* Skip zbuffer clears */
                if (sFillColor == 0xFFFCFFFC) break;

                c = sFillColor >> 16;
                r = ((c >> 11) & 0x1F) / 31.0f;
                g = ((c >> 6) & 0x1F) / 31.0f;
                b = ((c >> 1) & 0x1F) / 31.0f;

                glDisable(GL_TEXTURE_2D);
                glColor3f(r, g, b);
                glBegin(GL_QUADS);
                glVertex2f(xl, yl);
                glVertex2f(xh, yl);
                glVertex2f(xh, yh);
                glVertex2f(xl, yh);
                glEnd();
            } break;

            case G_SETSCISSOR:
                /* TODO: implement scissor */
                break;

            case G_GEOMETRYMODE:
            case G_SETOTHERMODE_L:
            case G_SETOTHERMODE_H:
            case G_SETCOMBINE:
            case G_SETTILE:
            case G_LOADBLOCK:
            case G_MOVEMEM:
            case G_MOVEWORD:
            case G_SETZIMG:
            case G_SETCIMG:
                /* TODO: implement these */
                break;

            default:
                /* Unknown command */
                break;
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================
void pc_init_gfx(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow("Paper Mario",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    glctx = SDL_GL_CreateContext(window);

    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 320, 240, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    printf("OpenGL initialized\n");
}

void pc_process_displaylist(Gfx* dl) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mtx_identity(sModelview);
    mtx_identity(sProjection);
    mtx_identity(sMVP);
    sTexScaleS = sTexScaleT = 0xFFFF;

    walk_dl(dl, 0);

    //SDL_GL_SwapWindow(window);
}

void linux_main_loop(void) {
    SDL_Event event;

    printf("Entering main loop...\n");
    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("SDL_QUIT received\n");
                SDL_Quit();
                exit(0);
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                SDL_Quit();
                exit(0);
            }
        }

        gfxRetrace_Callback(0);
        SDL_Delay(33);
    }
}

int main(int argc, char* argv[]) {
    printf("Paper Mario PC starting...\n");
    pc_init_gfx();
    boot_main(NULL);
    return 0;
}
