#include "resource/factory/LightFactory.h"
#include "resource/type/Light.h"

std::shared_ptr<Ship::IResource> LUS::ResourceFactoryBinaryLightV0::ReadResource(std::shared_ptr<Ship::File> file) {
    if (!FileHasValidFormatAndReader(file)) {
        return nullptr;
    }

    std::shared_ptr<Light> light = std::make_shared<Light>(file->InitData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    reader->Read((char*) light->mLight.a.col, 3);
    memcpy((char*) light->mLight.a.colc, (char*) light->mLight.a.col, 3);

    uint32_t count = reader->ReadUInt32();
    for(size_t i = 0; i < count; i++){
        LightData data;
        reader->Read((char*) data.l.col,  3);
        memcpy((char*) data.l.colc, (char*) data.l.col, 3);
        reader->Read((char*) data.l.dir,  3);
        light->mLightData.push_back(data);
    }

    light->mLightData.push_back({});

    light->mLight.l = light->mLightData.data();
    return light;
}