#include "libultraship/libultraship.h"

extern "C" {

void osCreateViManager(OSPri pri) {
}

void osViSetEvent(OSMesgQueue* queue, OSMesg mesg, uint32_t c) {

    __OSEventState* es = &__osEventStateTab[OS_EVENT_VI];

    es->queue = queue;
    es->msg = mesg;
}

void osViSwapBuffer(void* a) {
}

void osViSetSpecialFeatures(uint32_t a) {
}

void osViSetMode(OSViMode* a) {
}

void osViBlack(uint8_t a) {
}

void* osViGetNextFramebuffer(void) {
    return nullptr;
}

void* osViGetCurrentFramebuffer(void) {
    return nullptr;
}

void osViSetXScale(float a) {
}

void osViSetYScale(float a) {
}

}