#include "libultraship/libultraship.h"

extern "C" {

int32_t osGbpakInit(OSMesgQueue* queue, OSPfs* pfs, int channel) {
    return PFS_ERR_NOPACK;
}

int32_t osGbpakPower(OSPfs* pfs, int32_t flag) {
    return PFS_ERR_NOPACK;
}

int32_t osGbpakReadId(OSPfs* pfs, OSGbpakId* id, uint8_t* status) {
    return PFS_ERR_NOPACK;
}

int32_t osGbpakReadWrite(OSPfs* pfs, uint16_t flag, uint16_t address, uint8_t* buffer, uint16_t size) {
    return PFS_ERR_NOPACK;
}

int32_t osGbpakGetStatus(OSPfs* pfs, uint8_t* status) {
    return PFS_ERR_NOPACK;
}

int32_t osGbpakCheckConnector(OSPfs* pfs, uint8_t* status) {
    return PFS_ERR_NOPACK;
}

}