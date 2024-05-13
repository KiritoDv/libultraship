#pragma once

#include "engine/SDKContext.h"
#include "VI.h"
#include "DPC.h"
#include "MEM.h"

namespace Ship {

class UltraContext : LUS::SDKContext {
public:
    UltraContext();
    ~UltraContext();
    void Create() override;

    std::shared_ptr<VI>  vi;
    std::shared_ptr<DPC> dpc;
    std::shared_ptr<MEM> mem;
};
};