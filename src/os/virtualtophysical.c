#ifdef LINUX
#include <stdint.h>
#endif

#include "PR/osint.h"
#include "PR/R4300.h"

u32 osVirtualToPhysical(void *addr) {
#ifdef LINUX
    return (u32)(uintptr_t)addr;
#else
    if (IS_KSEG0(addr)) {
        return K0_TO_PHYS(addr);
    } else if (IS_KSEG1(addr)) {
        return K1_TO_PHYS(addr);
    } else {
        return __osProbeTLB(addr);
    }
#endif
}
