// src/linux/main.c
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Forward declarations instead of including common.h
typedef int32_t s32;
typedef uint32_t u32;
typedef uint64_t Gfx;

void boot_main(void* data);
void gfxRetrace_Callback(s32 gfxTaskNum);

static SDL_Window* window;
static SDL_Renderer* renderer;

void pc_init_gfx(void) {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Paper Mario",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        320, 240, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

void pc_process_displaylist(Gfx* dl) {
    int cmd_count = 0;
    int max_cmds = 200;

    printf("=== Display List @ %p ===\n", (void*)dl);

    while (cmd_count < max_cmds) {
        uint32_t w0 = ((uint32_t*)dl)[0];
        uint32_t w1 = ((uint32_t*)dl)[1];
        uint8_t cmd = (w0 >> 24) & 0xFF;

        printf("  [%3d] %02X: %08X %08X", cmd_count, cmd, w0, w1);

        switch (cmd) {
            case 0x00: printf(" (G_NOOP)"); break;
            case 0x01: printf(" (G_VTX) nvtx=%d", (w0 >> 12) & 0xFF); break;
            case 0x02: printf(" (G_MODIFYVTX)"); break;
            case 0x03: printf(" (G_CULLDL)"); break;
            case 0x04: printf(" (G_BRANCH_Z)"); break;
            case 0x05: printf(" (G_TRI1)"); break;
            case 0x06: printf(" (G_TRI2)"); break;
            case 0x07: printf(" (G_QUAD)"); break;
            case 0xD7: printf(" (G_TEXTURE)"); break;
            case 0xD8: printf(" (G_POPMTX)"); break;
            case 0xD9: printf(" (G_GEOMETRYMODE)"); break;
            case 0xDA: printf(" (G_MTX)"); break;
            case 0xDB: printf(" (G_MOVEWORD)"); break;
            case 0xDC: printf(" (G_MOVEMEM)"); break;
            case 0xDE: printf(" (G_DL) addr=%08X %s", w1, (w0 & 0xFF) ? "branch" : "call"); break;
            case 0xDF:
                printf(" (G_ENDDL)\n");
                printf("=== End DL (%d commands) ===\n", cmd_count + 1);
                return;
            case 0xE1: printf(" (G_RDPHALF_2)"); break;
            case 0xE2: printf(" (G_SETOTHERMODE_H)"); break;
            case 0xE3: printf(" (G_SETOTHERMODE_L)"); break;
            case 0xE4: printf(" (G_TEXRECT)"); break;
            case 0xE5: printf(" (G_TEXRECTFLIP)"); break;
            case 0xE6: printf(" (G_RDPLOADSYNC)"); break;
            case 0xE7: printf(" (G_RDPPIPESYNC)"); break;
            case 0xE8: printf(" (G_RDPTILESYNC)"); break;
            case 0xE9: printf(" (G_RDPFULLSYNC)"); break;
            case 0xEA: printf(" (G_SETKEYGB)"); break;
            case 0xEB: printf(" (G_SETKEYR)"); break;
            case 0xEC: printf(" (G_SETCONVERT)"); break;
            case 0xED: printf(" (G_SETSCISSOR)"); break;
            case 0xEE: printf(" (G_SETPRIMDEPTH)"); break;
            case 0xEF: printf(" (G_RDPSETOTHERMODE)"); break;
            case 0xF0: printf(" (G_LOADTLUT)"); break;
            case 0xF1: printf(" (G_RDPHALF_1)"); break;
            case 0xF2: printf(" (G_SETTILESIZE)"); break;
            case 0xF3: printf(" (G_LOADBLOCK)"); break;
            case 0xF4: printf(" (G_LOADTILE)"); break;
            case 0xF5: printf(" (G_SETTILE)"); break;
            case 0xF6: printf(" (G_FILLRECT)"); break;
            case 0xF7: printf(" (G_SETFILLCOLOR)"); break;
            case 0xF8: printf(" (G_SETFOGCOLOR)"); break;
            case 0xF9: printf(" (G_SETBLENDCOLOR)"); break;
            case 0xFA: printf(" (G_SETPRIMCOLOR)"); break;
            case 0xFB: printf(" (G_SETENVCOLOR)"); break;
            case 0xFC: printf(" (G_SETCOMBINE)"); break;
            case 0xFD: printf(" (G_SETTIMG) fmt=%d addr=%08X", (w0 >> 21) & 7, w1); break;
            case 0xFE: printf(" (G_SETZIMG) addr=%08X", w1); break;
            case 0xFF: printf(" (G_SETCIMG) addr=%08X", w1); break;
            default: printf(" (unknown)"); break;
        }
        printf("\n");

        dl++;
        cmd_count++;
    }

    printf("=== DL truncated at %d commands ===\n", max_cmds);
}

void gfx_swap_buffers(void) {
    SDL_RenderPresent(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
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
        }

        gfxRetrace_Callback(0);
        SDL_Delay(16);
    }
}

int main(int argc, char* argv[]) {
    printf("Paper Mario PC starting...\n");
    pc_init_gfx();
    boot_main(NULL);
    return 0;
}
