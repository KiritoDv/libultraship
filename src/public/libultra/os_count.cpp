#include "libultraship/libultraship.h"

extern "C" {

//The real libultra APIs return 21.33ns ticks (48.875mhz) so to be compatible we would need to multiply
//the returned values by 46,882.

//PD uses these APIs to calculate weather effects.


uint64_t osGetTime(void) {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

uint32_t osGetCount(void) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}