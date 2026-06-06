cmake_minimum_required(VERSION 3.24.0)

# Enable /Z7 (embedded debug info) instead of /Zi (shared PDB) on MSVC so that
# sccache can cache object files without racing over a shared .pdb file.
# CMP0141 was introduced in CMake 3.25, so guard it for older versions.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

option(NON_PORTABLE "Build a non-portable version" OFF)
option(DISABLE_DLL_LOADER "Disable dynamic library loading support" OFF)
option(ENABLE_SCRIPTING "Enable TCC-based C mod compilation and scripting system" OFF)

if(EMSCRIPTEN)
    set(ENABLE_SCRIPTING OFF CACHE BOOL "Scripting disabled for Emscripten" FORCE)
    include(FetchContent)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
    )
    set(_zlib_save_shared ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_MakeAvailable(zlib)
    set(BUILD_SHARED_LIBS ${_zlib_save_shared})

    set(ZLIB_FOUND          TRUE       CACHE BOOL   "" FORCE)
    set(ZLIB_INCLUDE_DIR    "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
    set(ZLIB_INCLUDE_DIRS   "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
    set(ZLIB_VERSION_STRING "1.3.1")

    # Create ZLIB::ZLIB as a STATIC IMPORTED target rather than an ALIAS.
    # An ALIAS resolves to zlibstatic (a built target), which causes libzip's
    # export() to require zlibstatic in the export set.  An IMPORTED target is
    # treated as an external library by CMake's export machinery and avoids
    # this requirement.  add_dependencies ensures zlibstatic is built first.
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB STATIC IMPORTED GLOBAL)
        # zlib sets OUTPUT_NAME z on both zlib and zlibstatic, so the actual
        # static archive is libz.a (not libzlibstatic.a).
        set_target_properties(ZLIB::ZLIB PROPERTIES
            IMPORTED_LOCATION             "${zlib_BINARY_DIR}/libz.a"
            INTERFACE_INCLUDE_DIRECTORIES "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}"
        )
        add_dependencies(ZLIB::ZLIB zlibstatic)
    endif()
    # Point the legacy variable to the IMPORTED target name so find_package
    # callers (libzip's internal find_package(ZLIB)) use it correctly.
    set(ZLIB_LIBRARY        ZLIB::ZLIB CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES      ZLIB::ZLIB)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    option(SIGN_LIBRARY "Enable xcode signing" OFF)
    option(BUNDLE_ID "Bundle ID for xcode signing" "com.example.libultraship")
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
endif()

project(libultraship LANGUAGES C CXX)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	# Default to embedded debug info (/Z7) so sccache can cache object files
	# without multiple CL.EXE processes racing over a shared .pdb file.
	if(POLICY CMP0141)
		set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Embedded>")
	endif()
endif()

include(cmake/Utils.cmake)
set(ADDITIONAL_LIB_INCLUDES "")

# ========= Configuration Options =========
option(INCLUDE_MPQ_SUPPORT "Enable StormLib and MPQ archive support" OFF)
option(GBI_UCODE "Specify the GBI ucode version" F3DEX_GBI_2)

# =========== Dependencies =============
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(cmake/dependencies/windows-vcpkg.cmake)
endif()

include(cmake/dependencies/common.cmake)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
	include(cmake/dependencies/android.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(cmake/dependencies/mac.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(cmake/dependencies/ios.cmake)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(cmake/dependencies/linux.cmake)
endif()

if(EMSCRIPTEN)
    include(cmake/dependencies/emscripten.cmake)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    include(cmake/dependencies/openbsd.cmake)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(cmake/dependencies/windows.cmake)
endif()

# === Platform Specific Configuration ===
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(cmake/ios-toolchain-populate.cmake)
endif()

# =========== Sources =============
add_subdirectory("src")

# =========== Tests =============
option(LUS_BUILD_TESTS "Build unit tests" OFF)
if(LUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory("tests")
endif()
