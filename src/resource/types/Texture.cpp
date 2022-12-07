#include "Texture.h"

namespace Ship {
void TextureV0::ParseFileBinary(BinaryReader* reader, Resource* res) {
    Texture* tex = (Texture*)res;

    ResourceFile::ParseFileBinary(reader, tex);

    tex->texType = (TextureType)reader->ReadUInt32();
    tex->width = reader->ReadUInt32();
    tex->height = reader->ReadUInt32();

    uint32_t dataSize = reader->ReadUInt32();

    tex->imageDataSize = dataSize;
    tex->imageData = new uint8_t[dataSize];

    reader->Read((char*)tex->imageData, dataSize);
}

void TextureV1::ParseFileBinary(BinaryReader* reader, Resource* res) {
    Texture* tex = (Texture*)res;

    ResourceFile::ParseFileBinary(reader, tex);

    tex->texType = (TextureType)reader->ReadUInt32();
    tex->width = reader->ReadUInt32();
    tex->height = reader->ReadUInt32();
    tex->hasBiggerTMEM = reader->ReadInt8();

    uint32_t dataSize = reader->ReadUInt32();

    tex->imageDataSize = dataSize;
    tex->imageData = new uint8_t[dataSize];
    reader->Read((char*) tex->imageData, dataSize);
}
} // namespace Ship