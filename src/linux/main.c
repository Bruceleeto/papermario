#include "common.h"
#include <unistd.h>
#include <stdio.h>

void gfxRetrace_Callback(s32 gfxTaskNum);

void pc_process_displaylist(Gfx* dl) {
    // TODO: F3DEX2 interpreter
    (void)dl;
}

void gfx_swap_buffers(void) {
    // TODO: SDL_GL_SwapWindow
}

void linux_main_loop(void) {
    printf("Entering main loop...\n");
    while (1) {
        gfxRetrace_Callback(0);
        usleep(16667);  // ~60fps
    }
}

int main(int argc, char* argv[]) {
    printf("Paper Mario PC starting...\n");
    boot_main(NULL);
    return 0;
}
