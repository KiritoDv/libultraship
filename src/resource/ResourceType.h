#pragma once

namespace LUS {

enum class ResourceType {
    // Not set
    None = 0x00000000,

    // Common
    Archive = 0x4F415243,     // OARC (UNUSED)
    DisplayList = 0x4F444C54, // ODLT
    Vertex = 0x4F565458,      // OVTX
    Matrix = 0x4F4D5458,      // OMTX
    Array = 0x4F415252,       // OARR
    Blob = 0x4F424C42,        // OBLB
    Texture = 0x4F544558,     // OTEX
    Bank = 0x42414E4B,        // BANK
    Sample = 0x41554643,      // AIFC
    Sequence = 0x53455143,    // SEQC

    // SM64
    SAnim = 0x414E494D,       // ANIM
    SDialog = 0x53444C47,     // SDLG
    Dictionary = 0x44494354,  // DICT
};
} // namespace LUS
