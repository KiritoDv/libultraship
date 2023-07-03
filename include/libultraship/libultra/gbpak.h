#ifndef ULTRA64_GBPAK_H
#define ULTRA64_GBPAK_H

#include "pfs.h"

typedef struct {
    uint16_t fixed1;
    uint16_t start_address;
    uint8_t nintendo_chr[0x30];
    uint8_t game_title[16];
    uint16_t company_code;
    uint8_t body_code;
    uint8_t cart_type;
    uint8_t rom_size;
    uint8_t ram_size;
    uint8_t country_code;
    uint8_t fixed2;
    uint8_t version;
    uint8_t isum;
    uint16_t sum;
} OSGbpakId;


int32_t osGbpakInit(OSMesgQueue*, OSPfs*, int);
int32_t osGbpakPower(OSPfs*, int32_t);
int32_t osGbpakGetStatus(OSPfs*, uint8_t*);
int32_t osGbpakReadWrite(OSPfs*, uint16_t, uint16_t, uint8_t*, uint16_t);
int32_t osGbpakReadId(OSPfs*, OSGbpakId*, uint8_t*);
int32_t osGbpakCheckConnector(OSPfs*, uint8_t*);

#endif