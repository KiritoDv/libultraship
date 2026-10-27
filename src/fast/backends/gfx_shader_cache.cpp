#include "fast/backends/gfx_shader_cache.h"

#include <cstdio>
#include <fstream>
#include <iterator>

#include "ship/Context.h"
#include "spdlog/spdlog.h"

namespace Fast {

void ShaderDiskCache::Open(const std::string& backend) {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::path(Ship::Context::GetPathRelativeToAppDirectory("shadercache")) /
                                backend;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        SPDLOG_WARN("Shader cache disabled, cannot create {}: {}", dir.string(), ec.message());
        mDir.clear();
        return;
    }
    mDir = std::move(dir);
}

bool ShaderDiskCache::IsOpen() const {
    return !mDir.empty();
}

std::vector<char> ShaderDiskCache::Load(uint64_t key) const {
    if (!IsOpen()) {
        return {};
    }
    std::ifstream in(PathFor(key), std::ios::binary);
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

void ShaderDiskCache::Store(uint64_t key, const void* data, size_t size) const {
    if (!IsOpen() || size == 0) {
        return;
    }
    // Write beside the final name and rename, so an interrupted write never leaves a truncated shader.
    const std::filesystem::path path = PathFor(key);
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
}

uint64_t ShaderDiskCache::Hash(std::initializer_list<std::string_view> parts) {
    uint64_t hash = 0xCBF29CE484222325ULL;
    auto mix = [&hash](const void* data, size_t size) {
        for (size_t i = 0; i < size; i++) {
            hash = (hash ^ static_cast<const uint8_t*>(data)[i]) * 0x100000001B3ULL;
        }
    };
    for (std::string_view part : parts) {
        const uint64_t size = part.size();
        mix(&size, sizeof(size));
        mix(part.data(), part.size());
    }
    return hash;
}

std::filesystem::path ShaderDiskCache::PathFor(uint64_t key) const {
    char name[32];
    snprintf(name, sizeof(name), "%016llx.bin", static_cast<unsigned long long>(key));
    return mDir / name;
}

} // namespace Fast
