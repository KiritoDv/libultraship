#include "UltraContext.h"

void Ship::UltraContext::Create() {
    this->vi  = std::make_shared<VI>();
    this->dpc = std::make_shared<DPC>();
    this->mem = std::make_shared<MEM>();
}