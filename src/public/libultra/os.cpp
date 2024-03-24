#include "libultraship/libultraship.h"
#include "libultraship/src/Context.h"

#include <SDL2/SDL.h>

extern "C" {
uint8_t __osMaxControllers = MAXCONTROLLERS;

int32_t osContInit(OSMesgQueue* mq, uint8_t* controllerBits, OSContStatus* status) {
    *controllerBits = 0;

#ifndef __WIIU__
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        SPDLOG_ERROR("Failed to initialize SDL game controllers ({})", SDL_GetError());
        exit(EXIT_FAILURE);
    }

#ifndef __SWITCH__
    std::string controllerDb = LUS::Context::LocateFileAcrossAppDirs("gamecontrollerdb.txt");
    int mappingsAdded = SDL_GameControllerAddMappingsFromFile(controllerDb.c_str());
    if (mappingsAdded >= 0) {
        SPDLOG_INFO("Added SDL game controllers from \"{}\" ({})", controllerDb, mappingsAdded);
    } else {
        SPDLOG_ERROR("Failed add SDL game controller mappings from \"{}\" ({})", controllerDb, SDL_GetError());
    }
#endif
#endif

    LUS::Context::GetInstance()->GetControlDeck()->Init(controllerBits);

    return 0;
}

int32_t osContStartReadData(OSMesgQueue* mesg) {
    return 0;
}

void osContGetReadData(OSContPad* pad) {
    memset(pad, 0, sizeof(OSContPad) * __osMaxControllers);

    LUS::Context::GetInstance()->GetControlDeck()->WriteToPad(pad);
}

uint64_t osGetTime(void) {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

uint32_t osGetCount(void) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

s32 osEepromProbe(OSMesgQueue *) {
    return 0;
}
s32 osEepromRead(OSMesgQueue *, u8, u8 *) {
    return 0;
}
s32 osEepromWrite(OSMesgQueue *, u8, u8 *) {
    return 0;
}
s32 osEepromLongRead(OSMesgQueue *, u8, u8 *, int) {
    return 0;
}
s32 osEepromLongWrite(OSMesgQueue *, u8, u8 *, int) {
    return 0;
}

int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg){
    return 0;
}

OSPiHandle *osCartRomInit(void) {
    return NULL;
}

s32 osEPiStartDma(OSPiHandle *pihandle, OSIoMesg *mb, s32 direction) {
    return 0;
}

u32 osAiGetLength() {
    // TODO: Implement
    return 0;
}

s32 osAiSetNextBuffer(void *buff, u32 len) {
    // TODO: Implement
    return 0;
}

s32 __osMotorAccess(OSPfs* pfs, u32 vibrate) {
    return 0;
}

s32 osMotorInit(OSMesgQueue* ctrlrqueue, OSPfs* pfs, s32 channel) {
    return 0;
}
}

