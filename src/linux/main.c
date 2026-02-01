#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int u32;
typedef int s32;
typedef unsigned long long Gfx;

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
    (void)dl;
}

void gfx_swap_buffers(void) {
    SDL_RenderPresent(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void linux_main_loop(void) {
    SDL_Event event;
    int frame = 0;

    printf("Entering main loop...\n");
    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("SDL_QUIT received\n");
                SDL_Quit();
                exit(0);
            }
        }

        printf("--- Frame %d ---\n", frame++);
        gfxRetrace_Callback(0);
        SDL_Delay(16);

        if (frame > 100) {
            printf("Stopping after 100 frames for debug\n");
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    printf("Paper Mario PC starting...\n");
    pc_init_gfx();
    boot_main(NULL);
    return 0;
}
