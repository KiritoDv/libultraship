#include "libultraship/libultraship.h"

extern "C" {

int32_t osPfsIsPlug(OSMesgQueue* queue, uint8_t* pattern) {
    *pattern = 0;

    return 0;
}

int32_t osPfsInitPak(OSMesgQueue* queue, OSPfs* pfs, int32_t channel, int32_t* arg3) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsChecker(OSPfs* pfs) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsFileState(OSPfs* pfs, int32_t fileNo, OSPfsState* state) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsFindFile(OSPfs* pfs, uint16_t company_code, uint32_t game_code, uint8_t* game_name, uint8_t* ext_name, int32_t* file_no) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsNumFiles(OSPfs* pfs, int32_t* max_files, int32_t* files_used) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsFreeBlocks(OSPfs* pfs, int32_t* remaining) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsReadWriteFile(OSPfs* pfs, int32_t fileNo, uint8_t flag, int32_t offset, int32_t size, uint8_t* data) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsReSizeFile(OSPfs* pfs, uint16_t company_code, uint32_t game_code, uint8_t* game_name, uint8_t* ext_name, int32_t length) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsDeleteFile(OSPfs* pfs, uint16_t company_code, uint32_t game_code, uint8_t* game_name, uint8_t* ext_name) {
    return PFS_ERR_NOPACK;
}

int32_t osPfsAllocateFile(OSPfs* pfs, uint16_t company_code, uint32_t game_code, uint8_t* game_name, uint8_t* ext_name, int32_t num_bytes, int32_t* file_no) {
    return PFS_ERR_NOPACK;
}

}