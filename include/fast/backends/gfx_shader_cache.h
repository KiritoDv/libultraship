#pragma once

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Fast {

// Compiled shaders kept between runs, one file per key under shadercache/<backend>. The key must
// cover everything that affects the output, so a stale entry is never looked up again.
class ShaderDiskCache {
  public:
    void Open(const std::string& backend);
    bool IsOpen() const;
    std::vector<char> Load(uint64_t key) const;
    void Store(uint64_t key, const void* data, size_t size) const;

    static uint64_t Hash(std::initializer_list<std::string_view> parts);

  private:
    std::filesystem::path PathFor(uint64_t key) const;

    std::filesystem::path mDir;
};

} // namespace Fast
