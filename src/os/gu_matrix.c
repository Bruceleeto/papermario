#include "ultra64.h"

#define FTOFIX32(x) ((s32)((x) * 65536.0f))

/**
 * guMtxIdent - Fixed-point identity matrix
 */
void guMtxIdent(Mtx *m) {
    u32 *p = (u32 *)m;
    p[0]  = 0x00010000;
    p[1]  = 0x00000000;
    p[2]  = 0x00000001;
    p[3]  = 0x00000000;
    p[4]  = 0x00000000;
    p[5]  = 0x00010000;
    p[6]  = 0x00000000;
    p[7]  = 0x00000001;
    p[8]  = 0x00000000;
    p[9]  = 0x00000000;
    p[10] = 0x00000000;
    p[11] = 0x00000000;
    p[12] = 0x00000000;
    p[13] = 0x00000000;
    p[14] = 0x00000000;
    p[15] = 0x00000000;
}

/**
 * guMtxIdentF - Float identity matrix  
 */
void guMtxIdentF(f32 mf[4][4]) {
    u32 *p = (u32 *)mf;
    p[0]  = 0x3F800000;  // 1.0f
    p[1]  = 0x00000000;
    p[2]  = 0x00000000;
    p[3]  = 0x00000000;
    p[4]  = 0x00000000;
    p[5]  = 0x3F800000;
    p[6]  = 0x00000000;
    p[7]  = 0x00000000;
    p[8]  = 0x00000000;
    p[9]  = 0x00000000;
    p[10] = 0x3F800000;
    p[11] = 0x00000000;
    p[12] = 0x00000000;
    p[13] = 0x00000000;
    p[14] = 0x00000000;
    p[15] = 0x3F800000;
}

/**
 * guMtxF2L - Convert float matrix to fixed-point
 */
void guMtxF2L(f32 mf[4][4], Mtx *m) {
    s32 i;
    u32 *src = (u32 *)mf;
    u32 *dst = (u32 *)m;
    
    for (i = 0; i < 8; i++) {
        s32 e1 = FTOFIX32(((f32 *)src)[i * 2]);
        s32 e2 = FTOFIX32(((f32 *)src)[i * 2 + 1]);
        dst[i]     = (e1 & 0xFFFF0000) | ((u32)e2 >> 16);
        dst[i + 8] = (e1 << 16) | (e2 & 0xFFFF);
    }
}

/**
 * guMtxL2F - Convert fixed-point matrix to float
 */
void guMtxL2F(f32 mf[4][4], Mtx *m) {
    s32 i;
    u32 *src = (u32 *)m;
    f32 *dst = (f32 *)mf;
    
    for (i = 0; i < 8; i++) {
        u32 intPart = src[i];
        u32 fracPart = src[i + 8];
        dst[i * 2]     = (s32)((intPart & 0xFFFF0000) | (fracPart >> 16)) / 65536.0f;
        dst[i * 2 + 1] = (s32)((intPart << 16) | (fracPart & 0xFFFF)) / 65536.0f;
    }
}

/**
 * guMtxCatF - Matrix multiply: ab = a * b
 */
void guMtxCatF(f32 a[4][4], f32 b[4][4], f32 ab[4][4]) {
    f32 temp[4][4];
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            temp[i][j] = a[i][0] * b[0][j] +
                         a[i][1] * b[1][j] +
                         a[i][2] * b[2][j] +
                         a[i][3] * b[3][j];
        }
    }

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ab[i][j] = temp[i][j];
        }
    }
}

/**
 * guScaleF - Float scale matrix
 */
void guScaleF(f32 mf[4][4], f32 x, f32 y, f32 z) {
    guMtxIdentF(mf);
    mf[0][0] = x;
    mf[1][1] = y;
    mf[2][2] = z;
}

/**
 * guTranslateF - Float translation matrix
 */
void guTranslateF(f32 mf[4][4], f32 x, f32 y, f32 z) {
    guMtxIdentF(mf);
    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
}

/**
 * guScale - Fixed-point scale matrix
 */
void guScale(Mtx *m, f32 x, f32 y, f32 z) {
    f32 mf[4][4];
    guScaleF(mf, x, y, z);
    guMtxF2L(mf, m);
}

/**
 * guTranslate - Fixed-point translation matrix
 */
void guTranslate(Mtx *m, f32 x, f32 y, f32 z) {
    f32 mf[4][4];
    guTranslateF(mf, x, y, z);
    guMtxF2L(mf, m);
}
