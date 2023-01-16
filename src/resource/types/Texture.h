#pragma once

#include "../Resource.h"

#define TEX_FLAG_LOAD_AS_RAW (1 << 0)

namespace Ship {
enum class TextureType {
    Error = 0,
    RGBA32bpp = 1,
    RGBA16bpp = 2,
    Palette4bpp = 3,
    Palette8bpp = 4,
    Grayscale4bpp = 5,
    Grayscale8bpp = 6,
    GrayscaleAlpha4bpp = 7,
    GrayscaleAlpha8bpp = 8,
    GrayscaleAlpha16bpp = 9,
};

class TextureV0 : public ResourceFile {
  public:
    TextureType texType;
    uint16_t width, height;
    uint32_t offsetToImageData;
    uint32_t offsetToPaletteData;

    void ParseFileBinary(BinaryReader* reader, Resource* res) override;
};

class TextureV1 : public ResourceFile {
  public:
    TextureType texType;
    uint16_t width, height;
    uint32_t offsetToImageData;
    uint32_t offsetToPaletteData;

    void ParseFileBinary(BinaryReader* reader, Resource* res) override;
};

class Texture : public Resource {
  public:
    TextureType texType;
    uint16_t width, height;
    uint32_t imageDataSize;
    uint16_t oWidth, oHeight;
    uint32_t texFlags = 0;
    uint8_t* imageData;
    uint8_t* paletteData;
};
} // namespace Ship
