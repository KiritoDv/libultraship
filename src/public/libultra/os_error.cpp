#include "libultraship/libultraship.h"

extern "C" {

void __osError(int16_t a, int16_t b, ...) {
}

OSThread* __osGetCurrFaultedThread() {
    return nullptr;
}

}
