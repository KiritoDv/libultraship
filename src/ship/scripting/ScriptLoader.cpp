#include "ship/scripting/ScriptLoader.h"

#ifdef ENABLE_SCRIPTING

#include "libultraship/bridge/eventsbridge.h"
#include "ship/events/Events.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/Archive.h"
#include "ship/resource/File.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <optional>
#include <sstream>
#include <string_view>
#ifndef DISABLE_TCC_COMPILER
#include <libtcc.h>
#endif
#include <memory>
#include <queue>
#include <unordered_map>
#include <filesystem>

namespace Ship {
std::optional<std::vector<uint8_t>> LoadFromO2R(const std::string& path,
                                                const std::shared_ptr<Archive>& archive = nullptr) {
    const auto file = archive->LoadFile(path);
    if (file == nullptr || !file->IsLoaded) {
        SPDLOG_ERROR("Failed to load script file: {}", path);
        return std::nullopt;
    }

    return std::vector<uint8_t>(file->Buffer->begin(), file->Buffer->end());
}

constexpr std::string_view Trim(const std::string_view v) {
    constexpr std::string_view whitespace = " \t\r\n";

    const auto start = v.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        return {};
    }

    const auto end = v.find_last_not_of(whitespace);
    return v.substr(start, end - start + 1);
}

constexpr std::string_view GetPlatform() {
#if defined(_WIN32) || defined(_WIN64)
#if defined(_M_ARM64) || defined(__aarch64__)
    return "windows_arm64";
#else
    return "windows_x64";
#endif

#elif defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
    return "ios";
#elif TARGET_OS_MAC
    return "darwin";
#endif

#elif defined(__ANDROID__)
    return "android";

#elif defined(__linux__)
#if defined(__x86_64__) || defined(_M_X64)
    return "linux_x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "linux_x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "linux_arm64";
#else
    return "linux_generic";
#endif

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    return "bsd";
#else
#error "Unsupported Operating System"
#endif
}

static std::vector<uint32_t> ParseVersion(std::string_view version) {
    std::vector<uint32_t> parts;
    uint32_t value = 0;
    bool digits = false;
    for (const char c : version) {
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            digits = true;
        } else if (c == '.') {
            parts.push_back(value);
            value = 0;
            digits = false;
        } else {
            break;
        }
    }
    if (digits || parts.empty()) {
        parts.push_back(value);
    }
    return parts;
}

static int CompareVersions(std::string_view a, std::string_view b) {
    const auto left = ParseVersion(a);
    const auto right = ParseVersion(b);
    for (size_t i = 0; i < std::max(left.size(), right.size()); i++) {
        const uint32_t l = i < left.size() ? left[i] : 0;
        const uint32_t r = i < right.size() ? right[i] : 0;
        if (l != r) {
            return l < r ? -1 : 1;
        }
    }
    return 0;
}

// A range is a list of comparisons separated by spaces or commas, e.g. ">=1.2 <2". A version without an operator
// is a minimum.
static bool VersionInRange(std::string_view version, std::string_view range) {
    size_t pos = 0;
    while (pos < range.size()) {
        const size_t start = range.find_first_not_of(" ,", pos);
        if (start == std::string_view::npos) {
            break;
        }
        const size_t end = std::min(range.find_first_of(" ,", start), range.size());
        std::string_view term = range.substr(start, end - start);
        pos = end;

        std::string_view op = ">=";
        for (const std::string_view candidate : { ">=", "<=", ">", "<", "=" }) {
            if (term.starts_with(candidate)) {
                op = candidate;
                term.remove_prefix(candidate.size());
                break;
            }
        }
        const int cmp = CompareVersions(version, term);
        const bool ok = op == ">="   ? cmp >= 0
                        : op == "<=" ? cmp <= 0
                        : op == ">"  ? cmp > 0
                        : op == "<"  ? cmp < 0
                                     : cmp == 0;
        if (!ok) {
            return false;
        }
    }
    return true;
}

void ScriptLoader::Compile(const std::shared_ptr<Archive>& archive) {
    const ArchiveManifest& info = archive->GetManifest();
    constexpr std::string_view platform = GetPlatform();
    const bool isCodeMod = !info.Main.empty() || !info.Binaries.empty();

    if (mSafeLevel == SafeLevel::DISABLE_SCRIPTS) {
        SPDLOG_WARN("Script loading is disabled. Skipping script from archive: {}", archive->GetPath());
        return;
    }

    if (!isCodeMod) {
        return;
    }

    if (info.CodeVersion != mCodeVersion) {
        throw std::runtime_error("built for code version " + std::to_string(info.CodeVersion) +
                                 ", this build needs code version " + std::to_string(mCodeVersion));
    }

    if (info.ApiVersion > mApiVersion) {
        throw std::runtime_error("needs API version " + std::to_string(info.ApiVersion) + ", this build has " +
                                 std::to_string(mApiVersion));
    }

    const bool isTrusted = archive->IsSigned() && archive->IsChecksumValid();

    if (mSafeLevel == SafeLevel::ONLY_TRUSTED_SCRIPTS && !isTrusted) {
        throw std::runtime_error(
            "Script loading is disabled for untrusted scripts. Failed to load script from archive: " +
            archive->GetPath());
    }

    if (mSafeLevel == SafeLevel::WARN_UNTRUSTED_SCRIPTS) {
        if (isTrusted) {
            SPDLOG_INFO("Loaded trusted script from archive: {}", archive->GetPath());
        } else {
            SPDLOG_WARN(
                "Loaded untrusted script from archive: {}. This script is not signed or has an invalid checksum. "
                "This may be a security risk if you do not trust the source of this script.",
                archive->GetPath());
        }
    }

    mLoadedArchives.push_back(archive);

    Scripting::LibraryLoader loader;

    const auto& binaries = info.Binaries;

    if (binaries.contains(std::string(platform))) {
        const std::string& path = binaries.at(std::string(platform));
        auto data = LoadFromO2R(path, archive);
        if (!data.has_value()) {
            throw std::runtime_error("Failed to load platform-specific binary: " + path);
        }

        // Prebuilt platform binary: written to a temp file and loaded through the
        // platform dynamic linker. These are real shared libraries the mod author
        // shipped (and can code-sign), so the loader path is unavoidable here.
        const std::string temp = loader.GenerateTempFile();
        loader.WriteToTempFile(*data);
        loader.Init(temp);
    } else if (!info.Main.empty()) {
#ifdef DISABLE_TCC_COMPILER
        SPDLOG_WARN("ScriptLoader: '{}' ships C sources but the TCC compiler is disabled; provide a prebuilt "
                    "binary for this platform",
                    info.Name);
        return;
#else
        const auto data = LoadFromO2R(info.Main, archive);
        if (!data.has_value()) {
            throw std::runtime_error("Failed to load main script: " + info.Main);
        }

        TCCState* s = tcc_new();
        if (!s) {
            throw std::runtime_error("Failed to create TCCState");
        }

        std::string errorLog;

        tcc_set_error_func(s, &errorLog, [](void* opaque, const char* msg) {
            std::string_view sv(msg);
            if (sv.find("warning") != std::string_view::npos) {
                SPDLOG_WARN("Compiler: {}", msg);
            } else if (sv.find("error") != std::string_view::npos || sv.find("fatal") != std::string_view::npos) {
                SPDLOG_ERROR("Compiler: {}", msg);
                auto* log = static_cast<std::string*>(opaque);
                if (!log->empty()) {
                    log->push_back('\n');
                }
                *log += msg;
            } else {
                SPDLOG_INFO("Compiler: {}", msg);
            }
        });

        tcc_define_symbol(s, "__DLL__", "1");

        for (const auto& [key, value] : mCompileDefines) {
            tcc_define_symbol(s, key.c_str(), value.c_str());
        }

        tcc_set_options(s, mBuildOptions.c_str());

        // Tell TCC where its own includes and libtcc1.a live.
        // We look for a library path whose parent has an "include" sibling
        // (the canonical .tcc/ root layout).
        for (const auto& libPath : mLibraryPaths) {
            auto tccRoot = std::filesystem::path(libPath).parent_path();
            if (std::filesystem::exists(tccRoot / "include")) {
                tcc_set_lib_path(s, tccRoot.string().c_str());
                break;
            }
        }

        // Compile into memory rather than emitting a shared library: the code is
        // JIT-relocated in-process (see InitInMemory below), so no executable is
        // written to disk.
        tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

        for (const std::string& includePath : mIncludePaths) {
            if (!std::filesystem::exists(includePath)) {
                SPDLOG_WARN("Include path does not exist: {}", includePath);
                continue;
            }

            if (!std::filesystem::is_directory(includePath)) {
                SPDLOG_WARN("Include path is not a directory: {}", includePath);
                continue;
            }

            tcc_add_include_path(s, includePath.c_str());
        }

        for (const std::string& libraryPath : mLibraryPaths) {
            if (!std::filesystem::exists(libraryPath)) {
                SPDLOG_WARN("Library path does not exist: {}", libraryPath);
                continue;
            }

            if (!std::filesystem::is_directory(libraryPath)) {
                SPDLOG_WARN("Library path is not a directory: {}", libraryPath);
                continue;
            }

            tcc_add_library_path(s, libraryPath.c_str());
        }

        for (const std::string& library : mLibraries) {
            tcc_add_library(s, library.c_str());
        }

        const std::vector<uint8_t>& raw = data.value();
        std::string content(raw.begin(), raw.end());
        std::istringstream stream(content);
        std::string line;

        while (std::getline(stream, line)) {
            if (line.empty()) {
                continue;
            }

            line.erase(line.find_last_not_of(" \r\n\t") + 1);
            line.erase(0, line.find_first_not_of(" \r\n\t"));

            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::string safePath = line;

            auto buf = LoadFromO2R(safePath, archive);
            if (!buf.has_value()) {
                tcc_delete(s);
                throw std::runtime_error("Failed to load script file: '" + safePath + "'");
            }

            std::string lineFixer = "#line 1 \"" + GetSourcePrefix(info.Name) + safePath + "\"\n";
            std::string sourceCode = lineFixer + std::string(buf->begin(), buf->end());
            if (tcc_compile_string(s, sourceCode.c_str()) == -1) {
                tcc_delete(s);
                throw std::runtime_error(errorLog.empty() ? "TCC Error in " + safePath : errorLog);
            }
        }

        // Relocate the compiled unit into executable memory and hand ownership of
        // the TCCState to the loader. No shared library is written to disk and
        // neither dlopen() nor LoadLibrary() is called, which avoids the
        // "drop an executable to disk and load it" pattern antivirus flags.
        try {
            loader.InitInMemory(s);
        } catch (const std::exception& e) {
            tcc_delete(s);
            // tcc_relocate() reports its own diagnostics (undefined symbols,
            // missing runtime objects such as bt-log.o) through the error
            // callback, so surface them alongside the generic failure.
            throw std::runtime_error(errorLog.empty() ? std::string(e.what()) : std::string(e.what()) + "\n" + errorLog);
        }
#endif // DISABLE_TCC_COMPILER
    }

    mLoadedScripts[info.Name] = loader;
};

void ScriptLoader::CompileAll(const std::optional<std::function<void(const std::shared_ptr<Archive>&)>>& preCallback,
                              const std::optional<std::function<void()>>& postCallback) {
    auto archive = mResourceManager->GetArchiveManager();
    auto list = archive->GetArchives();

    for (const auto& entry : *list) {
        const auto& info = entry->GetManifest();
        if (info.Main.empty() && info.Binaries.empty()) {
            continue;
        }

        if (preCallback.has_value()) {
            preCallback.value()(entry);
        }
        Compile(entry);
        if (postCallback.has_value()) {
            postCallback.value()();
        }
    }
}

std::vector<std::string> ScriptLoader::GetLoadersInDependencyOrder(std::vector<std::string>* errors) const {
    std::unordered_map<std::string, std::shared_ptr<Archive>> archiveMap;
    for (const auto& entry : mLoadedArchives) {
        const auto& info = entry->GetManifest();
        if (mLoadedScripts.contains(info.Name)) {
            archiveMap[info.Name] = entry;
        }
    }

    const auto skip = [&](const std::string& name, const std::string& reason) {
        SPDLOG_ERROR("Skipping script {}: {}", name, reason);
        if (errors != nullptr) {
            errors->push_back(name + ": " + reason);
        }
        archiveMap.erase(name);
    };

    for (bool changed = true; changed;) {
        changed = false;
        std::vector<std::pair<std::string, std::string>> skipped;
        for (const auto& [name, entry] : archiveMap) {
            const auto& info = entry->GetManifest();
            for (const std::string& depName : info.Dependencies) {
                const auto dep = archiveMap.find(depName);
                if (dep == archiveMap.end()) {
                    skipped.emplace_back(name, "needs " + depName + ", which is missing or did not load");
                    break;
                }
                const auto range = info.DependencyVersions.find(depName);
                const std::string& version = dep->second->GetManifest().Version;
                if (range != info.DependencyVersions.end() && !VersionInRange(version, range->second)) {
                    skipped.emplace_back(name, "needs " + depName + " " + range->second + ", found " + version);
                    break;
                }
            }
        }
        for (const auto& [name, reason] : skipped) {
            skip(name, reason);
            changed = true;
        }
    }

    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto& [name, entry] : archiveMap) {
        inDegree.try_emplace(name, 0);
        for (const std::string& depName : entry->GetManifest().Dependencies) {
            dependents[depName].push_back(name);
            inDegree[name]++;
        }
    }

    std::queue<std::string> readyQueue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0) {
            readyQueue.push(name);
        }
    }

    std::vector<std::string> loadOrder;
    while (!readyQueue.empty()) {
        std::string current = readyQueue.front();
        readyQueue.pop();

        loadOrder.push_back(current);

        for (const std::string& dependentName : dependents[current]) {
            inDegree[dependentName]--;

            if (inDegree[dependentName] == 0) {
                readyQueue.push(dependentName);
            }
        }
    }

    for (const auto& [name, degree] : inDegree) {
        if (degree > 0) {
            skip(name, "its dependencies form a cycle");
        }
    }

    return loadOrder;
}

void ScriptLoader::CallEntry(const std::string& name, const std::string& symbol) {
    const auto entry = (Scripting::LibraryFunc_t)mLoadedScripts.at(name).GetFunction(symbol);
    if (mEntryHook) {
        mEntryHook(name, symbol, entry);
    } else if (entry) {
        entry();
    }
}

void ScriptLoader::RemoveListeners(const std::string& name) {
    const auto events = EventSystemGetEvents();
    if (events == nullptr) {
        return;
    }
    const std::string prefix = GetSourcePrefix(name);
    const auto fromScript = [&prefix](const char* path) {
        return path != nullptr && std::string_view(path).starts_with(prefix);
    };
    size_t removed = 0;
    for (auto& [id, registration] : events->GetEventRegistrations()) {
        removed += std::erase_if(registration.Listeners,
                                 [&](const EventListener& listener) { return fromScript(listener.Metadata.Path); });
        std::erase_if(registration.Callers, [&](const auto& caller) { return fromScript(caller.first); });
    }
    if (removed > 0) {
        SPDLOG_INFO("Removed {} event listeners left by script {}", removed, name);
    }
}

void ScriptLoader::LoadAll() {
    mLoadErrors.clear();
    for (const auto& id : GetLoadersInDependencyOrder(&mLoadErrors)) {
        if (std::find(mInitializedScripts.begin(), mInitializedScripts.end(), id) != mInitializedScripts.end()) {
            continue;
        }
        SPDLOG_INFO("Initializing script: {}", id);
        mInitializedScripts.push_back(id);
        CallEntry(id, "ModInit");
    }
}

void ScriptLoader::UnloadAll() {
    for (auto it = mInitializedScripts.rbegin(); it != mInitializedScripts.rend(); ++it) {
        SPDLOG_INFO("Uninitialize script: {}", *it);
        CallEntry(*it, "ModExit");
    }
    for (auto& [name, loader] : mLoadedScripts) {
        RemoveListeners(name);
        loader.Unload();
    }
    mInitializedScripts.clear();
    mLoadedScripts.clear();
}

const std::vector<std::string>& ScriptLoader::GetLoadErrors() const {
    return mLoadErrors;
}

void ScriptLoader::SetEntryHook(EntryHook hook) {
    mEntryHook = std::move(hook);
}

void ScriptLoader::SetApiVersion(uint32_t apiVersion) {
    mApiVersion = apiVersion;
}

std::string ScriptLoader::GetSourcePrefix(const std::string& name) {
    return "[" + name + "]:";
}

void* ScriptLoader::GetFunction(const std::string& name, const std::string& function) {
    if (mLoadedScripts.contains(name)) {
        void* fn = mLoadedScripts.at(name).GetFunction(function);
        if (fn == nullptr) {
            SPDLOG_WARN("ScriptLoader::GetFunction: symbol '{}' not found in module '{}'", function, name);
        }
        return fn;
    }
    SPDLOG_WARN("ScriptLoader::GetFunction: module '{}' not loaded (looking for '{}')", name, function);
    return nullptr;
};

} // namespace Ship

#endif // ENABLE_SCRIPTING