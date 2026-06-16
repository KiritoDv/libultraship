#include "ship/scripting/ScriptLoader.h"

#ifdef ENABLE_SCRIPTING

#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/Archive.h"
#include "ship/resource/File.h"
#include "spdlog/spdlog.h"
#include <optional>
#include <sstream>
#include <string_view>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <iomanip>

// Clang/LLVM headers for in-process C compilation
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallVector.h>
#if LLVM_VERSION_MAJOR >= 17
#include <llvm/TargetParser/Host.h>
#else
#include <llvm/Support/Host.h>
#endif

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"

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

void ScriptLoader::SetCacheDir(const std::filesystem::path& dir) {
    mCacheDir = dir;
}

std::filesystem::path ScriptLoader::GetCachePath(const ArchiveManifest& manifest) const {
    if (mCacheDir.empty() || manifest.Checksum.empty()) {
        return {};
    }

    // Build a cache key from content identity + compiler configuration.
    // Any change to the archive bytes, code version, or build flags will
    // produce a different key and force a recompile.
    std::string raw = manifest.Checksum + ":" + std::to_string(mCodeVersion) + ":" + mBuildOptions;

    // Fold the string into a 64-bit hash (platform-independent hex string).
    std::size_t h = 0;
    for (unsigned char c : raw) {
        h = h * 31 + c;
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;

#if defined(_WIN32) || defined(__CYGWIN__)
    return mCacheDir / (oss.str() + ".dll");
#elif defined(__APPLE__)
    return mCacheDir / (oss.str() + ".dylib");
#else
    return mCacheDir / (oss.str() + ".so");
#endif
}

void ScriptLoader::StoreInCache(const ArchiveManifest& manifest, const std::string& srcPath) const {
    const auto cachePath = GetCachePath(manifest);
    if (cachePath.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (ec) {
        SPDLOG_WARN("ScriptLoader: failed to create cache directory {}: {}", cachePath.parent_path().string(),
                    ec.message());
        return;
    }

    std::filesystem::copy_file(srcPath, cachePath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        SPDLOG_WARN("ScriptLoader: failed to write cache entry {}: {}", cachePath.string(), ec.message());
    } else {
        SPDLOG_INFO("ScriptLoader: cached compiled script to {}", cachePath.string());
    }
}

// ---------------------------------------------------------------------------

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
        SPDLOG_ERROR("Incompatible code version for archive {}: expected {}, got {}", archive->GetPath(), mCodeVersion,
                     info.CodeVersion);
        return;
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
    const std::string temp = loader.GenerateTempFile();

    if (info.Binaries.find(std::string(platform)) == info.Binaries.end() && !info.Main.empty()) {
        const auto cachePath = GetCachePath(info);
        if (!cachePath.empty() && std::filesystem::exists(cachePath)) {
            SPDLOG_INFO("ScriptLoader: cache hit for '{}', skipping recompile ({})", info.Name, cachePath.string());
            std::error_code ec;
            std::filesystem::copy_file(cachePath, temp, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) {
                loader.Init(temp);
                mLoadedScripts[info.Name] = loader;
                return;
            }
            SPDLOG_WARN("ScriptLoader: failed to restore cache entry, recompiling: {}", ec.message());
        }
    }

    if (binaries.contains(std::string(platform))) {
        const std::string& path = binaries.at(std::string(platform));
        auto data = LoadFromO2R(path, archive);
        if (!data.has_value()) {
            throw std::runtime_error("Failed to load platform-specific binary: " + path);
        }

        loader.WriteToTempFile(*data);
    } else if (!info.Main.empty()) {
        const auto data = LoadFromO2R(info.Main, archive);
        if (!data.has_value()) {
            throw std::runtime_error("Failed to load main script: " + info.Main);
        }

        // Initialize LLVM native target once per process.
        // Use the C++ inline functions from TargetSelect.h, not the C macros.
        static std::once_flag sLLVMInit;
        std::call_once(sLLVMInit, []() {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm::InitializeNativeTargetAsmParser();
        });

        // Write all C source files from the archive to a temp directory so
        // the Clang Driver can read them as ordinary files.
        const std::string tempDirStr =
            (std::filesystem::temp_directory_path() /
             ("lus_modcc_" + std::to_string(std::hash<std::string>{}(info.Name + info.Checksum))))
                .string();
        {
            std::error_code ec;
            std::filesystem::create_directories(tempDirStr, ec);
            if (ec) {
                throw std::runtime_error("Failed to create temp dir for mod compilation: " + ec.message());
            }
        }

        // RAII guard: always remove the temp dir when we leave this scope.
        struct TempDirGuard {
            const std::string& path;
            ~TempDirGuard() {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        } tempGuard{ tempDirStr };

        // Parse main file — it is a text list of source-file paths (one per
        // line), identical to the old TCC path list format.
        std::vector<std::string> sourceFiles;
        {
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

                auto buf = LoadFromO2R(line, archive);
                if (!buf.has_value()) {
                    throw std::runtime_error("Failed to load script file: '" + line + "'");
                }

                // Write to temp file; prepend a #line directive so Clang
                // diagnostic messages name the virtual archive path.
                auto srcFilePath = std::filesystem::path(tempDirStr) /
                                   std::filesystem::path(line).filename();
                std::ofstream out(srcFilePath, std::ios::binary);
                if (!out.is_open()) {
                    throw std::runtime_error("Failed to write temp source file: " + srcFilePath.string());
                }
                const std::string lineFixer =
                    "#line 1 \"[" + info.Name + "]:" + line + "\"\n";
                out.write(lineFixer.data(), static_cast<std::streamsize>(lineFixer.size()));
                out.write(reinterpret_cast<const char*>(buf->data()),
                          static_cast<std::streamsize>(buf->size()));
                out.close();
                sourceFiles.push_back(srcFilePath.string());
            }
        }

        if (sourceFiles.empty()) {
            throw std::runtime_error("No source files listed in main script for mod: " + info.Name);
        }

        // Build the Clang Driver argument vector.  This is equivalent to
        // invoking: clang -shared [-fPIC] -O1 -std=c11 <flags> <defines>
        //                 <includes> <libs> -o <temp.so> <src1.c> ...
        std::vector<std::string> argStrs;
        argStrs.push_back("clang"); // argv[0]: program name (arbitrary)
        argStrs.push_back("-shared");
#if !defined(_WIN32)
        argStrs.push_back("-fPIC");
#endif
        argStrs.push_back("-O1");
        argStrs.push_back("-std=c11");
        argStrs.push_back("-D__DLL__=1");

        // Pass user-supplied build flags verbatim (e.g. "-g").
        {
            std::istringstream opts(mBuildOptions);
            std::string opt;
            while (opts >> opt) {
                argStrs.push_back(std::move(opt));
            }
        }

        for (const auto& [key, val] : mCompileDefines) {
            argStrs.push_back("-D" + key + "=" + val);
        }
        for (const auto& p : mIncludePaths) {
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                argStrs.push_back("-I" + p);
            } else if (!p.empty()) {
                SPDLOG_WARN("ScriptLoader: include path does not exist: {}", p);
            }
        }
        for (const auto& p : mLibraryPaths) {
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                argStrs.push_back("-L" + p);
            } else if (!p.empty()) {
                SPDLOG_WARN("ScriptLoader: library path does not exist: {}", p);
            }
        }
        for (const auto& lib : mLibraries) {
            argStrs.push_back("-l" + lib);
        }
        argStrs.push_back("-o");
        argStrs.push_back(temp);
        for (const auto& src : sourceFiles) {
            argStrs.push_back(src);
        }

        std::vector<const char*> argv;
        argv.reserve(argStrs.size());
        for (const auto& s : argStrs) {
            argv.push_back(s.c_str());
        }

        // Set up a diagnostics engine that captures errors as a string so we
        // can surface them in the exception message.
        //
        // DiagnosticOptions API changed in LLVM 20:
        //   < 20: ref-counted (IntrusiveRefCntPtr); TextDiagnosticPrinter takes ptr
        //  >= 20: plain struct; TextDiagnosticPrinter takes reference
        std::string errorLog;
        llvm::raw_string_ostream errorOS(errorLog);
        llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs = new clang::DiagnosticIDs();
#if LLVM_VERSION_MAJOR >= 20
        clang::DiagnosticOptions diagOpts;
        auto* diagPrinter = new clang::TextDiagnosticPrinter(errorOS, diagOpts);
        clang::DiagnosticsEngine diagEngine(diagIDs, diagOpts, diagPrinter);
#else
        llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpts =
            new clang::DiagnosticOptions();
        auto* diagPrinter = new clang::TextDiagnosticPrinter(errorOS, diagOpts.get());
        clang::DiagnosticsEngine diagEngine(diagIDs, diagOpts, diagPrinter);
#endif

        // Determine the Clang resource directory (built-in headers).
        // Priority: bundled .clang/ next to the binary > build-time baked path.
        std::string resourceDir;
        for (const auto& libPath : mLibraryPaths) {
            std::filesystem::path parent = std::filesystem::path(libPath).parent_path();
            if (std::filesystem::exists(parent / "include")) {
                resourceDir = parent.string();
                break;
            }
        }
        if (resourceDir.empty()) {
            // Fall back to the directory baked in at build time.
            resourceDir = LUS_CLANG_RESOURCE_DIR;
        }

        clang::driver::Driver D("clang", llvm::sys::getDefaultTargetTriple(), diagEngine);
        D.setTitle("Ghostship ModCC");
        if (!resourceDir.empty()) {
            D.ResourceDir = resourceDir;
        }

        std::unique_ptr<clang::driver::Compilation> C(
            D.BuildCompilation(llvm::ArrayRef<const char*>(argv)));

        if (!C || diagEngine.hasErrorOccurred()) {
            errorOS.flush();
            throw std::runtime_error(errorLog.empty() ? "Clang: failed to build compilation job for " +
                                                             info.Name
                                                       : errorLog);
        }

        llvm::SmallVector<std::pair<int, const clang::driver::Command*>> failingCmds;
        D.ExecuteCompilation(*C, failingCmds);
        errorOS.flush();

        if (diagEngine.hasErrorOccurred() || !failingCmds.empty()) {
            throw std::runtime_error(
                errorLog.empty() ? "Clang: compilation failed for " + info.Name : errorLog);
        }

        StoreInCache(info, temp);
    }

    loader.Init(temp);
    mLoadedScripts[info.Name] = loader;
};

void ScriptLoader::CompileAll(const std::optional<std::function<void(const std::shared_ptr<Archive>&)>>& preCallback,
                              const std::optional<std::function<void()>>& postCallback) {
    auto archive = Context::GetInstance()->GetResourceManager()->GetArchiveManager();
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

std::vector<std::string> ScriptLoader::GetLoadersInDependencyOrder() const {
    std::vector<std::string> orderedLoaders;

    std::vector<std::string> loadOrder;
    std::unordered_map<std::string, std::shared_ptr<Archive>> archiveMap;
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;

    for (const auto& entry : mLoadedArchives) {
        const auto& info = entry->GetManifest();

        if (!mLoadedScripts.contains(info.Name)) {
            continue;
        }

        archiveMap[info.Name] = entry;
        inDegree[info.Name] = 0;
    }

    for (const auto& [name, entry] : archiveMap) {
        const auto& deps = entry->GetManifest().Dependencies;
        for (const std::string& depName : deps) {
            if (archiveMap.contains(depName)) {
                dependents[depName].push_back(name);
                inDegree[name]++;
            } else {
                throw std::runtime_error("Loaded archive " + name +
                                         " depends on missing or unloaded archive: " + depName);
            }
        }
    }

    std::queue<std::string> readyQueue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0) {
            readyQueue.push(name);
        }
    }

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

    if (loadOrder.size() != archiveMap.size()) {
        throw std::runtime_error("Circular dependency detected among loaded scripts. Failed to resolve init order.");
    }

    for (const std::string& name : loadOrder) {
        orderedLoaders.push_back(name);
    }

    return orderedLoaders;
}

void ScriptLoader::LoadAll() {
    auto loaders = GetLoadersInDependencyOrder();
    for (const auto& id : loaders) {
        auto& loader = mLoadedScripts.at(id);
        SPDLOG_INFO("Initializing script: {}", id);
        const auto init = (Scripting::LibraryFunc_t)loader.GetFunction("ModInit");
        if (init) {
            init();
        }
    }
}
void ScriptLoader::UnloadAll() {
    auto loaders = GetLoadersInDependencyOrder();
    for (auto it = loaders.rbegin(); it != loaders.rend(); ++it) {
        auto& loader = mLoadedScripts.at(*it);
        SPDLOG_INFO("Uninitialize script: {}", *it);
        const auto exit = (Scripting::LibraryFunc_t)loader.GetFunction("ModExit");

        if (exit) {
            exit();
        }

        loader.Unload();
    }
    mLoadedScripts.clear();
}

void* ScriptLoader::GetFunction(const std::string& name, const std::string& function) {
    if (mLoadedScripts.contains(name)) {
        return mLoadedScripts.at(name).GetFunction(function);
    }
    return nullptr;
};

} // namespace Ship

#endif // ENABLE_SCRIPTING