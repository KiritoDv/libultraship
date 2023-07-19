#pragma once

#ifndef WINDOWBRIDGE_H
#define WINDOWBRIDGE_H

#include "stdint.h"
#include "libultraship/libultra/gbi.h"


#ifdef __cplusplus
extern "C" {
#endif

uint32_t WindowGetWidth();
uint32_t WindowGetHeight();
float WindowGetAspectRatio();
void WindowGetPixelDepthPrepare(float x, float y);
uint16_t WindowGetPixelDepth(float x, float y);
void WindowStartFrame();
void WindowEndFrame();
void WindowRunDisplayList(Gfx* displayList);

#ifdef __cplusplus
};
#endif

#endif