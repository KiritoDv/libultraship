#include "libultraship/libultraship.h"
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
    const char* controllerDb = "gamecontrollerdb.txt";
    int mappingsAdded = SDL_GameControllerAddMappingsFromFile(controllerDb);
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

int32_t osContStartQuery(OSMesgQueue* mq) {

    OSMesg msg = { 0 };
    osSendMesg(mq, msg, OS_MESG_NOBLOCK);

    return 0;
}

void osContGetQuery(OSContStatus* data) {

    uint8_t controllerMask = *LUS::Context::GetInstance()->GetControlDeck()->GetControllerBits();

    for (int i = 0; i < MAXCONTROLLERS; i++) {
        if (controllerMask & (1 << i)) {
            data[i].type = CONT_TYPE_NORMAL;
            data[i].status = 0;
            data[i].err_no = 0;
        } else {
            data[i].type = 0;
            data[i].status = 0;
            data[i].err_no = CONT_NO_RESPONSE_ERROR;
        }
    }
}

int32_t osContStartReadData(OSMesgQueue* mesg) {

    OSMesg msg = { 0 };
    osSendMesg(mesg, msg, OS_MESG_NOBLOCK);

    return 0;
}

void osContGetReadData(OSContPad* pad) {
    memset(pad, 0, sizeof(OSContPad) * __osMaxControllers);

    LUS::Context::GetInstance()->GetControlDeck()->WriteToPad(pad);
}
}