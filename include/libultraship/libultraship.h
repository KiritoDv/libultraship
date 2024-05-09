#ifndef _LIBULTRASHIP_H_
#define _LIBULTRASHIP_H_

#include "libultra.h"
#include "bridge.h"
#include "color.h"
#include "luslog.h"

#ifdef __cplusplus
#include "classes.h"
#endif

#if defined(_WIN32)
    #define ALIGN_ASSET(x) __declspec(align(x))
#else
    #define ALIGN_ASSET(x) __attribute__((aligned (x)))
#endif

#endif
