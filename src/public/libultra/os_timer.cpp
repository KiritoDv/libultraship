#include "libultraship/libultraship.h"

//WiiU doesn't have SDL.
#ifndef __WIIU__
std::map<OSTimer*, SDL_TimerID> __lusTimerIdMap;
#endif

std::recursive_mutex __lusTimerMapLock;

#define TIMER_ID(t) (__lusTimerIdMap[t])

extern "C" {

uint32_t __lusTimerCallback(uint32_t interval, void* param) {

    OSTimer* t = (OSTimer*)param;

    osSendMesg(t->mq, t->msg, OS_MESG_NOBLOCK);

    return t->interval & 0xffffffff;
}

int32_t osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg) {
    
    std::lock_guard<std::recursive_mutex> lock(__lusTimerMapLock);

    osStopTimer(t);

    t->interval = interval;

    if (countdown != 0) {
        t->value = countdown;
    } else {
        t->value = interval;
    }

    t->mq = mq;
    t->msg = msg;

    double us = (double)OS_CYCLES_TO_USEC(t->value);

    uint32_t ms = (uint32_t)round(us / 1000.0);

#ifndef __WIIU__
    __lusTimerIdMap[t] = SDL_AddTimer(ms, __lusTimerCallback, t);
#endif

    return 0;
}

void osStopTimer(OSTimer* t) {

    std::lock_guard<std::recursive_mutex> lock(__lusTimerMapLock);

#ifndef __WIIU__
    if (__lusTimerIdMap.contains(t))
    {
        SDL_RemoveTimer(TIMER_ID(t));
        __lusTimerIdMap.erase(t);
    }
#endif
}

}