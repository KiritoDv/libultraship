#include "libultraship/libultraship.h"

extern "C" {

void osSpTaskLoad(OSTask* tp) {
}

void osSpTaskStartGo(OSTask* tp) {
}

void osSpTaskYield(void) {
}

OSYieldResult osSpTaskYielded(OSTask* tp) {
    return 0;
}

}
