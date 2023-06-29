#ifndef ULTRA64_EEPROM_H
#define ULTRA64_EEPROM_H

#include "message.h"

#define EEPROM_TYPE_4K 0x01
#define EEPROM_TYPE_16K 0x02

extern int32_t osEepromProbe(OSMesgQueue*);
extern int32_t osEepromLongRead(OSMesgQueue*, uint8_t, uint8_t*, int32_t);
extern int32_t osEepromLongWrite(OSMesgQueue*, uint8_t, uint8_t*, int32_t);

#endif