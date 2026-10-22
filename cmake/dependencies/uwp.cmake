target_sources(ImGui
	PRIVATE
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(ImGui PUBLIC SDL2::SDL2)

target_compile_definitions(ImGui PUBLIC
	IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
	IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
)
