#include "libultraship/libultraship.h"


typedef void(__lusThreadEntryPoint)(void*);

std::map<OSThread*, std::pair<__lusThreadEntryPoint*, void*>> __lusThreadParamMap;

class stop_thread_exception : public std::exception {};

extern "C" {

void __lusThreadTrampoline(OSThread* t, __lusThreadEntryPoint* entry, void* arg) {

    t->state = OS_STATE_RUNNING;

    try {
        entry(arg);
    }
    catch (stop_thread_exception) {
    }
}

void osCreateThread(OSThread* t, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri p) {

    memset(t, 0, sizeof(OSThread));

    t->id = id;
    t->priority = p;
    t->state = OS_STATE_STOPPED;

    __lusThreadParamMap[t] = std::make_pair(entry, arg);
}

void osStartThread(OSThread* t) {

    t->state = OS_STATE_RUNNABLE;
    
    __lusThreadEntryPoint* entrypoint = __lusThreadParamMap[t].first;
    void* arg = __lusThreadParamMap[t].second;

    __lusThreadParamMap.erase(t);

    std::thread thread = std::thread(__lusThreadTrampoline, t, entrypoint, arg);

    thread.detach();
}

void osStopThread(OSThread* t) {
    // osStopThread is a suspend thread rather than a terminate.

    // We can't safely implement this on a modern system because
    // the thread you are stopping/killing may be holding a lock.
}

OSPri osGetThreadPri(OSThread* thread) {
    return 0;
}

void osSetThreadPri(OSThread* t, OSPri pri) {
}

void osYieldThread(void) {
    std::this_thread::yield();
}

}