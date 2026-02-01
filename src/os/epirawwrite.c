#include "PR/piint.h"

s32 __osEPiRawWriteIo(OSPiHandle* pihandle, u32 devAddr, u32 data) {
#ifdef LINUX
    return 0;
#else
    u32 stat;
    u32 domain;

    EPI_SYNC(pihandle, stat, domain);
    IO_WRITE(pihandle->baseAddress | devAddr, data);

    return 0;
#endif
}
