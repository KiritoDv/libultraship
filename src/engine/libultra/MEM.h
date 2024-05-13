#pragma once

#include <cstdint>

namespace Ship {
struct MEM {
    uint8_t IMEM[1024 * 4];
    uint8_t DMEM[1024 * 4];
    uint8_t DRAM[1024 * 1024 * 32];
    uint32_t INTR_REG = 0;
};
};