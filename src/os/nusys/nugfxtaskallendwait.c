#include "common.h"
#include "nu/nusys.h"

void nuGfxTaskAllEndWait(void) {
#ifdef LINUX
    return;
#endif
    while (nuGfxTaskSpool) {
    }
}
