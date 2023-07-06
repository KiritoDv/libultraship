#include "libultraship/libultraship.h"

extern "C" {

void osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, int32_t cmdMsgCnt) {

}

int32_t osPiReadIo(uint32_t a, uint32_t* b) {
    return 0;
}

int32_t osPiWriteIo(uint32_t devAddr, uint32_t data) {
    return 0;
}

s32 osPiStartDma(OSIoMesg* a, int32_t b, int32_t c, uint32_t d, void* e, uint32_t f, OSMesgQueue* g) {
    return 0;
}

}
