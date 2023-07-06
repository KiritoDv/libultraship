#include "libultraship/libultraship.h"

extern "C" {

//This function should be called osMotorInit().
int32_t osMotorProbe(OSMesgQueue* ctrlrqueue, OSPfs* pfs, int32_t channel) {
    return PFS_ERR_NOPACK;
}

int32_t __osMotorAccess(OSPfs* a, uint32_t b) {
    return PFS_ERR_NOPACK;
}

}