#pragma once

#include <cstdint>

namespace Ship {
struct VI {
    uint32_t INTR_REG = 0;
    uint32_t LEAP_REG = 0;
    uint32_t WIDTH_REG = 0;
    uint32_t STATUS_REG = 0;
    uint32_t ORIGIN_REG = 0;
    uint32_t TIMING_REG = 0;
    uint32_t V_SYNC_REG = 0;
    uint32_t H_SYNC_REG = 0;
    uint32_t H_START_REG = 0;
    uint32_t V_START_REG = 0;
    uint32_t V_BURST_REG = 0;
    uint32_t X_SCALE_REG = 0;
    uint32_t Y_SCALE_REG = 0;
    uint32_t V_CURRENT_LINE_REG = 0;
};
};