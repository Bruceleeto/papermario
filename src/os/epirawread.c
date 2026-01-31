#include "PR/piint.h"

s32 __osEPiRawReadIo(OSPiHandle* pihandle, u32 devAddr, u32* data) {
#ifdef LINUX
    *data = 0;
    return 0;
#else
    u32 stat;
    u32 domain;

    EPI_SYNC(pihandle, stat, domain);
    *data = IO_READ(pihandle->baseAddress | devAddr);

    return 0;
#endif
}
