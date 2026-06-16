# lus_setup_clang_runtime(TARGET_NAME [RESOURCES_DIR <dest>])
#
# Stages Clang's built-in headers into <target_file_dir>/.clang/ so that
# ScriptLoader can find them at runtime when compiling C mods.
#
# The built-in headers (stddef.h, stdarg.h, float.h, …) are small (~200 KB)
# and live in CLANG_RESOURCE_DIR/include/.  They are distinct from system
# headers (stdio.h etc.) which Clang locates automatically via the host SDK.
#
# Parameters:
#   TARGET_NAME   — CMake target to attach POST_BUILD / install rules to.
#   RESOURCES_DIR — Install destination for the .clang/ tree.
#                   Defaults to ".clang" (next to the binary).
#                   Pass "../Resources/.clang" for macOS app bundles.

function(lus_setup_clang_runtime TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 LUS_CLANG "" "RESOURCES_DIR" "")

    if(NOT ENABLE_SCRIPTING)
        return()
    endif()

    if(NOT LUS_CLANG_RESOURCES_DIR)
        set(LUS_CLANG_RESOURCES_DIR ".clang")
    endif()

    if(NOT LUS_CLANG_RESOURCE_DIR AND NOT CLANG_RESOURCE_DIR)
        message(WARNING "lus_setup_clang_runtime: CLANG_RESOURCE_DIR not set — skipping runtime staging for ${TARGET_NAME}")
        return()
    endif()

    set(_res "${LUS_CLANG_RESOURCE_DIR}")
    if(NOT _res)
        set(_res "${CLANG_RESOURCE_DIR}")
    endif()

    set(_builtin_inc "${_res}/include")
    if(NOT EXISTS "${_builtin_inc}")
        message(WARNING "lus_setup_clang_runtime: built-in include dir not found at ${_builtin_inc}")
        return()
    endif()

    set(_stage "$<TARGET_FILE_DIR:${TARGET_NAME}>/.clang")

    # POST_BUILD: copy headers into the build tree so developers can run
    # from the build directory without installing.
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMENT "Staging Clang built-in headers for ${TARGET_NAME}..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_builtin_inc}" "${_stage}/include"
        VERBATIM
    )

    # Install rule: read directly from source so this never depends on the
    # staging step having run (same pattern as SetupTccRuntime.cmake).
    install(DIRECTORY "${_builtin_inc}/"
        DESTINATION "${LUS_CLANG_RESOURCES_DIR}/include"
        COMPONENT ${TARGET_NAME}
    )
endfunction()
