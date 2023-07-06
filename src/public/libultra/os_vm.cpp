#include "libultraship/libultraship.h"

extern "C" {

size_t osVirtualToPhysical(void* ptr) {
    return (size_t)ptr;
}

void osMapTLB(int32_t a, uint32_t b, void* c, uint32_t d, uint32_t e, uint32_t f) {
}

}
