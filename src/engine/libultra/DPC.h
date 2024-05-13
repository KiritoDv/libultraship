#pragma once

#include <cstdint>

namespace Ship {
struct DPC {
    uint32_t END_REG = 0;
    uint32_t TMEM_REG = 0;
    uint32_t START_REG = 0;
    uint32_t CLOCK_REG = 0;
    uint32_t STATUS_REG = 0;
    uint32_t CURRENT_REG = 0;
    uint32_t BUFBUSY_REG = 0;
    uint32_t PIPEBUSY_REG = 0;
};
};