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
    tex->hasMetadata = reader->ReadInt8();
    if(tex->hasMetadata){
        TextureMetadata* metadata = &tex->metadata;
        metadata->width = reader->ReadUInt32();
        metadata->height = reader->ReadUInt32();
        metadata->useBiggerTMEM = reader->ReadInt8();
    }
    uint32_t dataSize = reader->ReadUInt32();

    tex->imageDataSize = dataSize;
    tex->imageData = new uint8_t[dataSize];
    reader->Read((char*) tex->imageData, dataSize);
    for (int x = 0; x < tex->imageDataSize; x++) {
        printf("%d", tex->imageData[x]);
    }
}
} // namespace Ship