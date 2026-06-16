include(FetchContent)

find_package(OpenGL QUIET)

#=================== ImGui ===================
set(imgui_fixes_and_config_patch_file ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/patches/imgui-fixes-and-config.patch)
set(imgui_apply_patch_command ${CMAKE_COMMAND} -Dpatch_file=${imgui_fixes_and_config_patch_file} -Dwith_reset=TRUE -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/git-patch.cmake)

FetchContent_Declare(
    ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b-docking
    PATCH_COMMAND ${imgui_apply_patch_command}
)
FetchContent_MakeAvailable(ImGui)
list(APPEND ADDITIONAL_LIB_INCLUDES ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

add_library(ImGui STATIC)
set_property(TARGET ImGui PROPERTY CXX_STANDARD 20)

target_sources(ImGui
    PRIVATE
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
)

target_sources(ImGui
    PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)

target_include_directories(ImGui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends PRIVATE ${SDL2_INCLUDE_DIRS})

# ========= Vulkan =============
if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

    if(WIN32 AND NOT DEFINED ENV{VULKAN_SDK} AND EXISTS "C:/VulkanSDK")
        file(GLOB _lus_sdk_dirs LIST_DIRECTORIES true "C:/VulkanSDK/*")
        list(SORT _lus_sdk_dirs ORDER DESCENDING)
        foreach(_d IN LISTS _lus_sdk_dirs)
            if(IS_DIRECTORY "${_d}")
                set(ENV{VULKAN_SDK} "${_d}")
                message(STATUS "Auto-detected Vulkan SDK: ${_d}")
                break()
            endif()
        endforeach()
    endif()

    find_package(Vulkan QUIET)

    if(Vulkan_FOUND)
        set(LUS_ENABLE_VULKAN ON CACHE INTERNAL "Vulkan backend available")
        target_sources(ImGui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
        target_link_libraries(ImGui PUBLIC Vulkan::Vulkan)

            if(WIN32 AND DEFINED ENV{VULKAN_SDK})
            find_library(Vulkan_shaderc_shared_LIBRARY   NAMES shaderc_shared   HINTS "$ENV{VULKAN_SDK}/Lib" NO_DEFAULT_PATH)
            find_library(Vulkan_shaderc_shared_DEBUG_LIB NAMES shaderc_sharedd  HINTS "$ENV{VULKAN_SDK}/Lib" NO_DEFAULT_PATH)
            find_file(Vulkan_shaderc_shared_DLL       NAMES shaderc_shared.dll  HINTS "$ENV{VULKAN_SDK}/Bin" NO_DEFAULT_PATH)
            find_file(Vulkan_shaderc_shared_DEBUG_DLL NAMES shaderc_sharedd.dll HINTS "$ENV{VULKAN_SDK}/Bin" NO_DEFAULT_PATH)

            if(Vulkan_shaderc_shared_LIBRARY AND Vulkan_shaderc_shared_DLL)
                add_library(Vulkan::shaderc_shared SHARED IMPORTED GLOBAL)
                target_include_directories(Vulkan::shaderc_shared INTERFACE "$ENV{VULKAN_SDK}/Include")

                set_property(TARGET Vulkan::shaderc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
                set_target_properties(Vulkan::shaderc_shared PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${Vulkan_shaderc_shared_DLL}"
                    IMPORTED_IMPLIB_RELEASE   "${Vulkan_shaderc_shared_LIBRARY}")

                if(Vulkan_shaderc_shared_DEBUG_LIB AND Vulkan_shaderc_shared_DEBUG_DLL)
                    set_property(TARGET Vulkan::shaderc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                    set_target_properties(Vulkan::shaderc_shared PROPERTIES
                        IMPORTED_LOCATION_DEBUG "${Vulkan_shaderc_shared_DEBUG_DLL}"
                        IMPORTED_IMPLIB_DEBUG   "${Vulkan_shaderc_shared_DEBUG_LIB}")
                else()
                    set_property(TARGET Vulkan::shaderc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                    set_target_properties(Vulkan::shaderc_shared PROPERTIES
                        IMPORTED_LOCATION_DEBUG "${Vulkan_shaderc_shared_DLL}"
                        IMPORTED_IMPLIB_DEBUG   "${Vulkan_shaderc_shared_LIBRARY}")
                endif()

                message(STATUS "Vulkan rendering backend enabled (shaderc_shared DLL)")
            else()
                message(STATUS "Vulkan rendering backend enabled (shaderc_shared not found, shader compilation unavailable)")
            endif()
        else()
            if(DEFINED ENV{VULKAN_SDK})
                find_library(Vulkan_shaderc_combined_LIBRARY NAMES shaderc_combined HINTS "$ENV{VULKAN_SDK}/lib" "$ENV{VULKAN_SDK}/lib64" NO_DEFAULT_PATH)
                find_library(Vulkan_shaderc_combined_DEBUG_LIBRARY NAMES shaderc_combinedd HINTS "$ENV{VULKAN_SDK}/lib" "$ENV{VULKAN_SDK}/lib64" NO_DEFAULT_PATH)
            else()
                find_library(Vulkan_shaderc_combined_LIBRARY NAMES shaderc_combined)
                find_library(Vulkan_shaderc_combined_DEBUG_LIBRARY NAMES shaderc_combinedd)
            endif()

            if(Vulkan_shaderc_combined_LIBRARY)
                if(NOT TARGET Vulkan::shaderc_combined)
                    add_library(Vulkan::shaderc_combined STATIC IMPORTED GLOBAL)
                    set_target_properties(Vulkan::shaderc_combined PROPERTIES
                        IMPORTED_LOCATION "${Vulkan_shaderc_combined_LIBRARY}")
                    if(Vulkan_shaderc_combined_DEBUG_LIBRARY)
                        set_property(TARGET Vulkan::shaderc_combined APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                        set_target_properties(Vulkan::shaderc_combined PROPERTIES
                            IMPORTED_LOCATION_DEBUG "${Vulkan_shaderc_combined_DEBUG_LIBRARY}")
                    endif()
                endif()
                message(STATUS "Vulkan rendering backend enabled (shaderc_combined)")
            else()
                message(STATUS "Vulkan rendering backend enabled (shaderc not found, shader compilation unavailable)")
            endif()
        endif()
    else()
        set(LUS_ENABLE_VULKAN OFF CACHE INTERNAL "Vulkan backend available")
        message(STATUS "Vulkan rendering backend disabled")
    endif()
else()
    set(LUS_ENABLE_VULKAN OFF CACHE INTERNAL "Vulkan backend available")
endif()

# ========= StormLib =============
if(INCLUDE_MPQ_SUPPORT)
    set(stormlib_patch_file ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/patches/stormlib-optimizations.patch)
    set(stormlib_apply_patch_command ${CMAKE_COMMAND} -Dpatch_file=${stormlib_patch_file} -Dwith_reset=TRUE -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/git-patch.cmake)

    FetchContent_Declare(
        StormLib
        GIT_REPOSITORY https://github.com/ladislav-zezula/StormLib.git
        GIT_TAG v9.25
        PATCH_COMMAND ${stormlib_apply_patch_command}
    )
    FetchContent_MakeAvailable(StormLib)
    list(APPEND ADDITIONAL_LIB_INCLUDES ${stormlib_SOURCE_DIR}/src)
endif()

#=================== STB ===================
set(STB_DIR ${CMAKE_BINARY_DIR}/_deps/stb)
file(DOWNLOAD "https://github.com/nothings/stb/raw/0bc88af4de5fb022db643c2d8e549a0927749354/stb_image.h" "${STB_DIR}/stb_image.h")
file(WRITE "${STB_DIR}/stb_impl.c" "#define STB_IMAGE_IMPLEMENTATION\n#include \"stb_image.h\"")

add_library(stb STATIC)

target_sources(stb PRIVATE
    ${STB_DIR}/stb_image.h
    ${STB_DIR}/stb_impl.c
)

target_include_directories(stb PUBLIC ${STB_DIR})
list(APPEND ADDITIONAL_LIB_INCLUDES ${STB_DIR})

#=================== libgfxd ===================
if (GFX_DEBUG_DISASSEMBLER)
    FetchContent_Declare(
        libgfxd
        GIT_REPOSITORY https://github.com/glankk/libgfxd.git
        GIT_TAG 008f73dca8ebc9151b205959b17773a19c5bd0da
    )
    FetchContent_MakeAvailable(libgfxd)

    add_library(libgfxd STATIC)
    set_property(TARGET libgfxd PROPERTY C_STANDARD 11)

    target_sources(libgfxd PRIVATE
        ${libgfxd_SOURCE_DIR}/gbi.h
        ${libgfxd_SOURCE_DIR}/gfxd.h
        ${libgfxd_SOURCE_DIR}/priv.h
        ${libgfxd_SOURCE_DIR}/gfxd.c
        ${libgfxd_SOURCE_DIR}/uc.c
        ${libgfxd_SOURCE_DIR}/uc_f3d.c
        ${libgfxd_SOURCE_DIR}/uc_f3db.c
        ${libgfxd_SOURCE_DIR}/uc_f3dex.c
        ${libgfxd_SOURCE_DIR}/uc_f3dex2.c
        ${libgfxd_SOURCE_DIR}/uc_f3dexb.c
    )

    target_include_directories(libgfxd PUBLIC ${libgfxd_SOURCE_DIR})
endif()

#======== thread-pool ========
FetchContent_Declare(
    ThreadPool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
    GIT_TAG v4.1.0
)
FetchContent_MakeAvailable(ThreadPool)

list(APPEND ADDITIONAL_LIB_INCLUDES ${threadpool_SOURCE_DIR}/include)

#=========== prism ===========
option(PRISM_STANDALONE "Build prism as a standalone library" OFF)
FetchContent_Declare(
    prism
    GIT_REPOSITORY https://github.com/KiritoDv/prism-processor.git
    GIT_TAG aa8370981b2cf57c46172e6aa639d720137f9a92
)
FetchContent_MakeAvailable(prism)

# prism's CMakeLists.txt calls cmake_minimum_required(VERSION <3.10), which causes
# CMake to explicitly set CMP0141=OLD in prism's scope (cmake_policy(VERSION) disables
# all policies newer than the specified version).  With CMP0141=OLD the compile
# template always appends "/Fd <target>.pdb /FS" to every compile command.  sccache
# parses the command line, sees "/Fd prism.pdb", and expects that file to exist after
# compilation.  When our /Z7 override wins (no PDB written) sccache aborts with
# "failed to open file prism.pdb".
#
# The simplest fix is to compile prism without sccache at all (clear the launcher).
# With the launcher cleared, cl.exe compiles prism directly; /Z7 (embedded debug info)
# is still applied so no PDB file is written and no race over a shared .pdb occurs.
if(MSVC AND TARGET prism)
    set_target_properties(prism PROPERTIES
        C_COMPILER_LAUNCHER   ""
        CXX_COMPILER_LAUNCHER ""
    )
    target_compile_options(prism PRIVATE $<$<CONFIG:Debug>:/Z7>)
endif()

#=========== monocypher ===========
FetchContent_Declare(
    monocypher
    GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git
    GIT_TAG 0d85f98c9d9b0227e42cf795cb527dff372b40a4
)
FetchContent_MakeAvailable(monocypher)

add_library(monocypher STATIC)
set_property(TARGET monocypher PROPERTY C_STANDARD 11)

target_sources(monocypher PRIVATE
    ${monocypher_SOURCE_DIR}/src/monocypher.c
    ${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c
)

target_include_directories(monocypher PUBLIC 
    ${monocypher_SOURCE_DIR}/src
    ${monocypher_SOURCE_DIR}/src/optional
)

#=========== LLVM/Clang (scripting) ===========
# Replaces TCC. On Linux/macOS uses the system-installed LLVM/Clang dev
# libraries; on Windows, if no system install is found, automatically downloads
# the official pre-built LLVM release from GitHub (one-time, ~350 MB).
# Only the native target is compiled in, keeping binary size as small as
# possible relative to the full all-targets LLVM.
if(ENABLE_SCRIPTING)
    # The pre-built LLVM version used for the Windows auto-download.
    set(_lus_llvm_auto_version "18.1.8")

    # Search common install locations before falling back to system paths.
    set(_lus_llvm_hints
        "$ENV{LLVM_DIR}"
        # Homebrew on Apple Silicon
        "/opt/homebrew/opt/llvm/lib/cmake/llvm"
        # Homebrew on Intel macOS
        "/usr/local/opt/llvm/lib/cmake/llvm"
        # Ubuntu/Debian versioned packages (newest first)
        "/usr/lib/llvm-19/lib/cmake/llvm"
        "/usr/lib/llvm-18/lib/cmake/llvm"
        "/usr/lib/llvm-17/lib/cmake/llvm"
        "/usr/lib/llvm-16/lib/cmake/llvm"
        "/usr/lib/llvm-15/lib/cmake/llvm"
        "/usr/lib/llvm-14/lib/cmake/llvm"
        # LLVM Windows installer default path
        "C:/Program Files/LLVM/lib/cmake/llvm"
    )
    set(_lus_clang_hints
        "$ENV{Clang_DIR}"
        "/opt/homebrew/opt/llvm/lib/cmake/clang"
        "/usr/local/opt/llvm/lib/cmake/clang"
        "/usr/lib/llvm-19/lib/cmake/clang"
        "/usr/lib/llvm-18/lib/cmake/clang"
        "/usr/lib/llvm-17/lib/cmake/clang"
        "/usr/lib/llvm-16/lib/cmake/clang"
        "/usr/lib/llvm-15/lib/cmake/clang"
        "/usr/lib/llvm-14/lib/cmake/clang"
        # LLVM Windows installer default path
        "C:/Program Files/LLVM/lib/cmake/clang"
    )

    find_package(LLVM CONFIG QUIET HINTS ${_lus_llvm_hints})
    find_package(Clang CONFIG QUIET HINTS ${_lus_clang_hints})

    # ------------------------------------------------------------------ #
    #  Windows auto-download fallback                                      #
    # ------------------------------------------------------------------ #
    # If no system/user LLVM was found and we are on Windows, download the
    # official pre-built clang+llvm release from the LLVM GitHub releases.
    # The archive already contains LLVMConfig.cmake and ClangConfig.cmake,
    # so a second find_package() pointed at the extracted tree works
    # without building anything from source.
    #
    # WARNING: The archive is ~350 MB compressed / ~1.5 GB extracted.
    # It is cached by CMake's FetchContent so the download only happens
    # once per build directory.  Override _lus_llvm_auto_version above to
    # pin a different release.
    if(WIN32 AND (NOT LLVM_FOUND OR NOT Clang_FOUND))
        if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64" OR
           CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(_lus_llvm_win_triple "aarch64-pc-windows-msvc")
        else()
            set(_lus_llvm_win_triple "x86_64-pc-windows-msvc")
        endif()

        set(_lus_llvm_archive
            "clang+llvm-${_lus_llvm_auto_version}-${_lus_llvm_win_triple}")
        message(STATUS
            "Scripting: LLVM not found — downloading pre-built "
            "${_lus_llvm_archive} (~350 MB, cached after first run)...")

        FetchContent_Declare(llvm_prebuilt
            URL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${_lus_llvm_auto_version}/${_lus_llvm_archive}.tar.xz"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(llvm_prebuilt)

        # Point find_package at the extracted tree and re-run.
        set(LLVM_DIR  "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/llvm"  CACHE PATH "Auto-downloaded LLVM cmake dir"  FORCE)
        set(Clang_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/clang" CACHE PATH "Auto-downloaded Clang cmake dir" FORCE)

        find_package(LLVM  CONFIG REQUIRED HINTS "${LLVM_DIR}"  NO_DEFAULT_PATH)
        find_package(Clang CONFIG REQUIRED HINTS "${Clang_DIR}" NO_DEFAULT_PATH)
    endif()

    if(NOT LLVM_FOUND OR NOT Clang_FOUND)
        message(FATAL_ERROR
            "LLVM/Clang development libraries not found.\n"
            "ENABLE_SCRIPTING requires LLVM >= 14 with Clang headers installed.\n\n"
            "  macOS (Homebrew):  brew install llvm\n"
            "                     export LLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm\n"
            "  Ubuntu/Debian:     apt install llvm-dev libclang-dev clang\n"
            "  Windows:           auto-downloaded — check the CMake log for errors.\n\n"
            "Or set -DLLVM_DIR and -DClang_DIR explicitly to a compatible install.")
    endif()

    if(LLVM_PACKAGE_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "LLVM >= 14.0 required for scripting (found ${LLVM_PACKAGE_VERSION})")
    endif()

    message(STATUS "Scripting: LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
    message(STATUS "Scripting: Clang at ${Clang_DIR}")

    # Resolve the Clang resource directory (built-in headers like stddef.h).
    # We bake it in as a compile-time constant so ScriptLoader can locate the
    # bundled .clang/ headers without needing a real clang binary at runtime.
    if(NOT CLANG_RESOURCE_DIR)
        find_program(_lus_clang_exe
            NAMES clang-${LLVM_PACKAGE_VERSION} clang-19 clang-18 clang-17 clang-16 clang-15 clang-14 clang
            HINTS "${LLVM_TOOLS_BINARY_DIR}"
            NO_DEFAULT_PATH
        )
        if(_lus_clang_exe)
            execute_process(
                COMMAND "${_lus_clang_exe}" -print-resource-dir
                OUTPUT_VARIABLE CLANG_RESOURCE_DIR
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()
        if(NOT CLANG_RESOURCE_DIR)
            # Fall back: common layout relative to include dirs
            list(GET CLANG_INCLUDE_DIRS 0 _lus_clang_inc)
            get_filename_component(_lus_clang_root "${_lus_clang_inc}" DIRECTORY)
            set(CLANG_RESOURCE_DIR "${_lus_clang_root}/lib/clang/${LLVM_PACKAGE_VERSION}")
        endif()
    endif()
    message(STATUS "Scripting: Clang resource dir = ${CLANG_RESOURCE_DIR}")

    # Expose resource dir to ScriptLoader.cpp at compile time.
    set(LUS_CLANG_RESOURCE_DIR "${CLANG_RESOURCE_DIR}" CACHE INTERNAL "")
endif() # ENABLE_SCRIPTING
