// src/linux/main.c
#include <common.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


void boot_main(void* data);
void gfxRetrace_Callback(s32 gfxTaskNum);

// SDL/GL state
static SDL_Window* window;
static SDL_GLContext glctx;

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

// Current texture image (from G_SETTIMG)
static u32 sCurTexAddr = 0;
static u32 sCurTexFmt = 0;
static u32 sCurTexSiz = 0;
static u32 sCurTexWidth = 0;

// Active GL texture
static GLuint sCurTexID = 0;

// Tile descriptors
typedef struct {
    u32 fmt, siz, line, tmem, palette;
    u32 cms, cmt, masks, maskt, shifts, shiftt;
    // tile size (in 10.2 fixed point as stored)
    u32 uls, ult, lrs, lrt;
    // resolved texture address + dimensions for loaded tiles
    u32 texAddr;
    u32 texW, texH;
    GLuint texID;
} TileDescriptor;

static TileDescriptor sTiles[8];

// TLUT
static u16 sTLUT[256];

// RDPHALF storage for multi-word commands
static u32 sRDPHalf1 = 0;
static u32 sRDPHalf2 = 0;

static int sFrameCount = 0;

// Cycle type
static u32 sCycleType = 0; // G_CYC_1CYCLE etc

// ============================================================================
// ADDRESS TRANSLATION
// ============================================================================

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
    /* Do NOT swap64 - data is raw ROM bytes, only need per-element endian swap */

    if (fmt == G_IM_FMT_RGBA && siz == G_IM_SIZ_16b) {
        u16* s = (u16*)swapped;
 //       if (sFrameCount < 15 && w == 128) {
   //         u8* raw = swapped;
     //       printf("[TEX] first 16 raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
       //         raw[0],raw[1],raw[2],raw[3],raw[4],raw[5],raw[6],raw[7],
         //       raw[8],raw[9],raw[10],raw[11],raw[12],raw[13],raw[14],raw[15]);
      //  }
        for (i = 0; i < w * h; i++) {
            u16 raw = s[i];
            u16 p = (raw >> 8) | (raw << 8); /* BE to LE */
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
            u16 p = s[i];
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
            r = ((tlut >> 11) & 0x1F) << 3;
            g = ((tlut >> 6) & 0x1F) << 3;
            b = ((tlut >> 1) & 0x1F) << 3;
            a = (tlut & 1) ? 255 : 0;
            pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    } else if (fmt == G_IM_FMT_I && siz == G_IM_SIZ_4b) {
        for (i = 0; i < w * h; i++) {
            u8 v = (i & 1) ? (swapped[i/2] & 0xF) : (swapped[i/2] >> 4);
            v = (v << 4) | v;
            pixels[i] = (255 << 24) | (v << 16) | (v << 8) | v;
        }
    } else if (fmt == G_IM_FMT_IA && siz == G_IM_SIZ_4b) {
        for (i = 0; i < w * h; i++) {
            u8 v = (i & 1) ? (swapped[i/2] & 0xF) : (swapped[i/2] >> 4);
            u8 intensity = (v >> 1) & 0x7;
            u8 alpha = (v & 1) ? 255 : 0;
            intensity = (intensity << 5) | (intensity << 2) | (intensity >> 1);
            pixels[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
        }
    } else {
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

// Load a sub-region of the current texture image into a tile via G_LOADTILE.
// The coordinates are in 10.2 fixed point as the hardware uses.
static void tile_load_region(int tile, u32 uls, u32 ult, u32 lrs, u32 lrt) {
    TileDescriptor* t = &sTiles[tile];
    // The region to load from the source image (in texels)
    u32 sl = uls >> 2, tl = ult >> 2;
    u32 sh = lrs >> 2, th = lrt >> 2;
    u32 w = sh - sl + 1;
    u32 h = th - tl + 1;

    // Record source info on the render tile so G_TEXRECT can use it
    // The actual render tile is set up by a subsequent G_SETTILE + G_SETTILESIZE,
    // but the texture address comes from the last G_SETTIMG.
    t->texAddr = sCurTexAddr;
    t->texW = w;
    t->texH = h;

    // We don't actually build the GL texture here — we do it lazily when
    // the render tile is configured (G_SETTILESIZE) or when G_TEXRECT fires.
}

// Ensure a GL texture exists for the given tile using the full source image dimensions.
static void tile_ensure_texture(int tileIdx) {
    TileDescriptor* t = &sTiles[tileIdx];
    if (t->texID) return;
    if (!t->texAddr || !t->texW || !t->texH) return;
    t->texID = load_texture(t->texAddr, t->fmt, t->siz, t->texW, t->texH);
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

// Draw a textured or colored rectangle (handles G_TEXRECT)
static void draw_tex_rect(u32 xh, u32 yh, u32 tile, u32 xl, u32 yl,
                           u32 s, u32 t, u32 dsdx, u32 dtdy) {
    float x0 = xl / 4.0f;
    float y0 = yl / 4.0f;
    float x1 = xh / 4.0f;
    float y1 = yh / 4.0f;
    float s0, t0, s1, t1;
    TileDescriptor* td = &sTiles[tile & 7];
    float tw, th;
    s16 ss = (s16)s, st = (s16)t;
    s16 sdsdx = (s16)dsdx, sdtdy = (s16)dtdy;

  //  if (sFrameCount < 15) {
    //    printf("[DRAWTEX] rect=(%.1f,%.1f)-(%.1f,%.1f) tile=%u texID=%u texAddr=%p texW=%u texH=%u\n",
      //      x0, y0, x1, y1, tile, td->texID, (void*)(uintptr_t)td->texAddr, td->texW, td->texH);
   // }

    // Ensure the tile has a GL texture
    tile_ensure_texture(tile & 7);

    tw = (td->texW > 0) ? (float)td->texW : 1.0f;
    th = (td->texH > 0) ? (float)td->texH : 1.0f;

    // s,t are S10.5 fixed point; dsdx,dtdy are S5.10 fixed point
    s0 = (ss / 32.0f) / tw;
    t0 = (st / 32.0f) / th;
    s1 = s0 + ((x1 - x0) * sdsdx / 1024.0f) / tw;
    t1 = t0 + ((y1 - y0) * sdtdy / 1024.0f) / th;

    if (td->texID) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, td->texID);
    }
    glDisable(GL_DEPTH_TEST);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(s0, t0); glVertex2f(x0, y0);
    glTexCoord2f(s1, t0); glVertex2f(x1, y0);
    glTexCoord2f(s1, t1); glVertex2f(x1, y1);
    glTexCoord2f(s0, t1); glVertex2f(x0, y1);
    glEnd();
    glEnable(GL_DEPTH_TEST);
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
                break;

            case G_RDPHALF_1:
                sRDPHalf1 = w1;
                break;

            case G_RDPHALF_2:
                sRDPHalf2 = w1;
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
             //   if (sFrameCount < 15) {
               //     printf("[SETTIMG] addr=%p fmt=%d siz=%d w=%d\n", (void*)(uintptr_t)w1, sCurTexFmt, sCurTexSiz, sCurTexWidth);
              //  }
                break;

            case G_SETTILE: {
                int tileIdx = (w1 >> 24) & 0x7;
                TileDescriptor* td = &sTiles[tileIdx];
                td->fmt = (w0 >> 21) & 0x7;
                td->siz = (w0 >> 19) & 0x3;
                td->line = (w0 >> 9) & 0x1FF;
                td->tmem = w0 & 0x1FF;
                td->palette = (w1 >> 20) & 0xF;
                td->cmt = (w1 >> 18) & 0x3;
                td->maskt = (w1 >> 14) & 0xF;
                td->shiftt = (w1 >> 10) & 0xF;
                td->cms = (w1 >> 8) & 0x3;
                td->masks = (w1 >> 4) & 0xF;
                td->shifts = w1 & 0xF;
            } break;

            case G_LOADTILE: {
                int tileIdx = (w1 >> 24) & 0x7;
                u32 uls = (w0 >> 12) & 0xFFF;
                u32 ult = w0 & 0xFFF;
                u32 lrs = (w1 >> 12) & 0xFFF;
                u32 lrt = w1 & 0xFFF;
                tile_load_region(tileIdx, uls, ult, lrs, lrt);
                // Also propagate address to the render tile (tile 0)
                // since gDPLoadTextureTile loads via G_TX_LOADTILE then
                // sets up G_TX_RENDERTILE with a separate G_SETTILE+G_SETTILESIZE
                sTiles[0].texAddr = sCurTexAddr;
            } break;

            case G_LOADBLOCK:
                // Simplified: record dimensions from the block load
                // The subsequent G_SETTILE + G_SETTILESIZE will set up the render tile
                break;

            case G_LOADTLUT: {
                int count = ((w1 >> 14) & 0x3FF) + 1;
                void* data = pc_resolve_addr(sCurTexAddr);
                if (data && count <= 256) {
                    memcpy(sTLUT, data, count * 2);
                }
            } break;

            case G_SETTILESIZE: {
                int tileIdx = (w1 >> 24) & 0x7;
                TileDescriptor* td = &sTiles[tileIdx];
                td->uls = (w0 >> 12) & 0xFFF;
                td->ult = w0 & 0xFFF;
                td->lrs = (w1 >> 12) & 0xFFF;
                td->lrt = w1 & 0xFFF;

                // If this is the render tile and we have a loaded texture addr,
                // update dimensions and build the GL texture
                if (td->texAddr) {
                    u32 w = (td->lrs >> 2) - (td->uls >> 2) + 1;
                    u32 h = (td->lrt >> 2) - (td->ult >> 2) + 1;
                    td->texW = w;
                    td->texH = h;
                    if (td->texID) glDeleteTextures(1, &td->texID);
                    td->texID = load_texture(td->texAddr, td->fmt, td->siz, w, h);
                } else if (sCurTexAddr) {
                    // Fallback: use the global SETTIMG address
                    u32 w = (td->lrs >> 2) + 1;
                    u32 h = (td->lrt >> 2) + 1;
                    td->texAddr = sCurTexAddr;
                    td->texW = w;
                    td->texH = h;
                    if (td->texID) glDeleteTextures(1, &td->texID);
                    td->texID = load_texture(sCurTexAddr, sCurTexFmt, sCurTexSiz, w, h);
                }

                // Also keep legacy sCurTexID for 3D triangle path
                if (tileIdx == 0 && td->texID) {
                    sCurTexID = td->texID;
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, sCurTexID);
                }
            } break;

            case G_TEXRECT:
            case G_TEXRECTFLIP: {
            //    if (sFrameCount < 15) {
              //      printf("[TEXRECT] w0=0x%08X w1=0x%08X\n", w0, w1);
                //}
                // G_TEXRECT is 128 bits via gSPTextureRectangle:
                //   word0: cmd:8 | xh:12 | yh:12
                //   word1: tile:3 | xl:12 | yl:12
                //   next Gfx (G_RDPHALF_1): s:16 | t:16
                //   next Gfx (G_RDPHALF_2): dsdx:16 | dtdy:16
                u32 xh = (w0 >> 12) & 0xFFF;
                u32 yh = w0 & 0xFFF;
                u32 tile = (w1 >> 24) & 0x7;
                u32 xl = (w1 >> 12) & 0xFFF;
                u32 yl = w1 & 0xFFF;
                u32 stword, dword;
                u32 s_val, t_val, dsdx_val, dtdy_val;
                u8 nextCmd;

                // Read the next two Gfx words for s/t and dsdx/dtdy
                // They come as G_RDPHALF_1 and G_RDPHALF_2
                i++;
                nextCmd = (dl[i].words.w0 >> 24) & 0xFF;
                if (nextCmd == G_RDPHALF_1) {
                    stword = dl[i].words.w1;
                } else {
                    stword = dl[i].words.w0;
                }
                i++;
                nextCmd = (dl[i].words.w0 >> 24) & 0xFF;
                if (nextCmd == G_RDPHALF_2) {
                    dword = dl[i].words.w1;
                } else {
                    dword = dl[i].words.w0;
                }

                s_val = (stword >> 16) & 0xFFFF;
                t_val = stword & 0xFFFF;
                dsdx_val = (dword >> 16) & 0xFFFF;
                dtdy_val = dword & 0xFFFF;

                draw_tex_rect(xh, yh, tile, xl, yl, s_val, t_val, dsdx_val, dtdy_val);
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

                if (sFillColor == 0xFFFCFFFC) break;

                c = sFillColor >> 16;
                r = ((c >> 11) & 0x1F) / 31.0f;
                g = ((c >> 6) & 0x1F) / 31.0f;
                b = ((c >> 1) & 0x1F) / 31.0f;

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_TEXTURE_2D);
                glColor3f(r, g, b);
                glBegin(GL_QUADS);
                glVertex2f(xl, yl);
                glVertex2f(xh, yl);
                glVertex2f(xh, yh);
                glVertex2f(xl, yh);
                glEnd();
                glEnable(GL_DEPTH_TEST);
            } break;

            case G_SETOTHERMODE_H: {
                // Extract cycle type from othermode_h
                u32 shift = (w0 >> 8) & 0xFF;
                u32 len = (w0 & 0xFF) + 1;
                // Cycle type is bits 52-53 of othermode (shift=20, len=2)
                if (shift == 20 && len == 2) {
                    sCycleType = w1 >> 20;
                }
            } break;

            case G_SETSCISSOR:
            case G_GEOMETRYMODE:
            case G_SETOTHERMODE_L:
            case G_SETCOMBINE:
            case G_MOVEMEM:
            case G_MOVEWORD:
            case G_SETZIMG:
            case G_SETCIMG:
            case G_SETBLENDCOLOR:
            case G_SETFOGCOLOR:
            case G_SETPRIMDEPTH:
                break;

            default:
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

    memset(sTiles, 0, sizeof(sTiles));

    printf("OpenGL initialized\n");
}

void pc_process_displaylist(Gfx* dl) {
    int j;
    u8 firstCmd;

    if (sFrameCount == 8) {
        extern u8* gLogosImages;
        if (gLogosImages) {
            printf("[CHECK] gLogosImages=%p first bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                (void*)gLogosImages,
                gLogosImages[0], gLogosImages[1], gLogosImages[2], gLogosImages[3],
                gLogosImages[4], gLogosImages[5], gLogosImages[6], gLogosImages[7]);
            printf("[CHECK] SETTIMG addr 0x8fb1010 vs gLogosImages+0=%p\n", (void*)(gLogosImages + 0));
        } else {
            printf("[CHECK] gLogosImages is NULL!\n");
        }
    }

    // Detect if this is a background DL (starts with G_MOVEWORD 0xDB)
    // or a main DL (starts with G_MTX 0xDA). Only clear on background.
    firstCmd = (dl[0].words.w0 >> 24) & 0xFF;
    if (firstCmd == G_MOVEWORD || firstCmd == G_NOOP) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    mtx_identity(sModelview);
    mtx_identity(sProjection);
    mtx_identity(sMVP);
    sTexScaleS = sTexScaleT = 0xFFFF;
    sRDPHalf1 = sRDPHalf2 = 0;

    // Clean up previous frame's tile textures
    for (j = 0; j < 8; j++) {
        if (sTiles[j].texID) {
            glDeleteTextures(1, &sTiles[j].texID);
        }
        memset(&sTiles[j], 0, sizeof(TileDescriptor));
    }
    sCurTexID = 0;

    walk_dl(dl, 0);

    sFrameCount++;
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
