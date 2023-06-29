#include "libultraship/libultraship.h"

extern "C" {

int32_t osEepromProbe(OSMesgQueue* mq) {
    return EEPROM_TYPE_16K;
}

int32_t osEepromLongRead(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    
    memset(buffer, 0, length);

    return 0;
}

int32_t osEepromLongWrite(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    return 0;
}

}