include(FetchContent)

# ─────────────────────────────────────────────────────────────────────────────
# SDL2 — provided by the Emscripten port system via -sUSE_SDL=2.
# We create INTERFACE IMPORTED targets so the rest of the build system can
# reference SDL2::SDL2 / SDL2::SDL2-static / SDL2::SDL2main without needing
# a real SDL2 installation.  The -sUSE_SDL=2 flag makes emcc add the port's
# include dirs during compilation AND link against it during linking.
# ─────────────────────────────────────────────────────────────────────────────
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_COMPILE_OPTIONS "SHELL:-sUSE_SDL=2"
        INTERFACE_LINK_OPTIONS    "SHELL:-sUSE_SDL=2"
    )
endif()

if(NOT TARGET SDL2::SDL2-static)
    add_library(SDL2::SDL2-static INTERFACE IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2-static PROPERTIES
        INTERFACE_COMPILE_OPTIONS "SHELL:-sUSE_SDL=2"
        INTERFACE_LINK_OPTIONS    "SHELL:-sUSE_SDL=2"
    )
endif()

if(NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main INTERFACE IMPORTED GLOBAL)
endif()

# Wire ImGui (already created by common.cmake) to use the SDL2 stub so that
# -sUSE_SDL=2 is transitively propagated to imgui_impl_sdl2.cpp compilation
# and to the final link step.
target_link_libraries(ImGui PUBLIC SDL2::SDL2)

# Emscripten maps OpenGL ES 3 to WebGL 2 internally; there is no libGLESv2.so
# to link against.  We create a stub target so that libultraship/src/CMakeLists.txt's
#   target_link_libraries(libultraship PRIVATE GLESv2)
# doesn't cause a wasm-ld "library not found" error.
# The real WebGL 2 support comes from -sFULL_ES3=1 / -sUSE_WEBGL2=1 in the
# root CMakeLists.txt target_link_options block.
if(NOT TARGET GLESv2)
    add_library(GLESv2 INTERFACE IMPORTED GLOBAL)
endif()
if(NOT TARGET GLESv3)
    add_library(GLESv3 INTERFACE IMPORTED GLOBAL)
endif()

# ImGui's OpenGL ES 3 backend is selected by this definition.
target_compile_definitions(ImGui PRIVATE IMGUI_IMPL_OPENGL_ES3)

# ─────────────────────────────────────────────────────────────────────────────
# FetchContent fallbacks for packages required by libultraship/src/CMakeLists.txt.
# OVERRIDE_FIND_PACKAGE redirects any later find_package(<pkg> REQUIRED) to the
# FetchContent download when the package is not found in the Emscripten sysroot.
# ─────────────────────────────────────────────────────────────────────────────

#=================== nlohmann-json ===================
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

#=================== tinyxml2 ===================
find_package(tinyxml2 QUIET)
if(NOT tinyxml2_FOUND)
    set(tinyxml2_BUILD_TESTING OFF)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG 11.0.0
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(tinyxml2)
endif()

#=================== spdlog ===================
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.16.0
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

#=================== libzip ===================
find_package(libzip QUIET)
if(NOT libzip_FOUND)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_TOOLS     OFF)
    set(BUILD_REGRESS   OFF)
    set(BUILD_EXAMPLES  OFF)
    set(BUILD_DOC       OFF)
    set(BUILD_OSSFUZZ   OFF)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        libzip
        GIT_REPOSITORY https://github.com/nih-at/libzip.git
        GIT_TAG v1.11.4
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(libzip)
    list(APPEND ADDITIONAL_LIB_INCLUDES ${libzip_SOURCE_DIR}/lib ${libzip_BINARY_DIR})
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Compile definitions and warning suppressions for the Emscripten target.
# ─────────────────────────────────────────────────────────────────────────────
add_compile_definitions(
    USE_OPENGLES=1
    EXCLUDE_MPQ_SUPPORT
)

# Suppress warnings about __EMSCRIPTEN__ being redefined — Emscripten's own
# headers define some identifiers before our headers see them.
# Enable JS-based exception handling globally so all TUs in this library
# are ABI-compatible with the game target.
add_compile_options(-Wno-macro-redefined -fexceptions)
