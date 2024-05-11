#include "RT64Window.h"

#include "Context.h"
#include "public/bridge/consolevariablebridge.h"
#include "graphic/Fast3D/gfx_rendering_api.h"

#define HLSL_CPU
#include "hle/rt64_application.h"
#include "rt64_render_hooks.h"

#include <fstream>

namespace Fast {
RT64Window::RT64Window() : RT64Window(std::vector<std::shared_ptr<Ship::GuiWindow>>()) {
}

RT64Window::RT64Window(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows) : Ship::Window(guiWindows) {

}

RT64Window::~RT64Window() {
    SPDLOG_DEBUG("destruct RT64Window");
}

void RT64Window::Init() {
    RT64::Application::Core appCore{};
#if defined(_WIN32)
    // appCore.window = window_handle.window;
#elif defined(__linux__)
    appCore.window.display = window_handle.display;
    appCore.window.window = window_handle.window;
#endif
}

void RT64Window::SetTargetFps(int32_t fps) {

}

void RT64Window::SetMaximumFrameLatency(int32_t latency) {

}

void RT64Window::GetPixelDepthPrepare(float x, float y) {

}

uint16_t RT64Window::GetPixelDepth(float x, float y) {
    throw std::runtime_error("GetPixelDepth not implemented");
}

void RT64Window::InitWindowManager() {

}

void RT64Window::SetTextureFilter(FilteringMode filteringMode) {

}

void RT64Window::Close() {

}

void RT64Window::StartFrame() {

}

void RT64Window::EndFrame() {
}

void RT64Window::SetCursorVisibility(bool visible) {

}

uint32_t RT64Window::GetWidth() {
    throw std::runtime_error("GetWidth not implemented");
}

uint32_t RT64Window::GetHeight() {
    throw std::runtime_error("GetHeight not implemented");
}

int32_t RT64Window::GetPosX() {
    throw std::runtime_error("GetPosX not implemented");
}

int32_t RT64Window::GetPosY() {
    throw std::runtime_error("GetPosY not implemented");
}

uint32_t RT64Window::GetCurrentRefreshRate() {
    throw std::runtime_error("GetCurrentRefreshRate not implemented");
}

bool RT64Window::SupportsWindowedFullscreen() {
    throw std::runtime_error("SupportsWindowedFullscreen not implemented");
}

bool RT64Window::CanDisableVerticalSync() {
    throw std::runtime_error("CanDisableVerticalSync not implemented");
}

void RT64Window::SetResolutionMultiplier(float multiplier) {
    throw std::runtime_error("SetResolutionMultiplier not implemented");
}

void RT64Window::SetMsaaLevel(uint32_t value) {
    throw std::runtime_error("SetMsaaLevel not implemented");
}

void RT64Window::SetFullscreen(bool isFullscreen) {
    SaveWindowToConfig();
    throw std::runtime_error("SetFullscreen not implemented");
}

bool RT64Window::IsFullscreen() {
    throw std::runtime_error("IsFullscreen not implemented");
}

bool RT64Window::IsRunning() {
    throw std::runtime_error("IsRunning not implemented");
}

const char* RT64Window::GetKeyName(int32_t scancode) {
    throw std::runtime_error("GetKeyName not implemented");
}

bool RT64Window::KeyUp(int32_t scancode) {
    if (scancode ==
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.Fullscreen", Ship::KbScancode::LUS_KB_F11)) {
        Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
    }

    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(-1);
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_UP, static_cast<Ship::KbScancode>(scancode));
}

bool RT64Window::KeyDown(int32_t scancode) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_DOWN, static_cast<Ship::KbScancode>(scancode));
    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(scancode);

    return isProcessed;
}

void RT64Window::AllKeysUp(void) {
    Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(Ship::KbEventType::LUS_KB_EVENT_ALL_KEYS_UP,
                                                                         Ship::KbScancode::LUS_KB_UNKNOWN);
}

void RT64Window::OnFullscreenChanged(bool isNowFullscreen) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    if (isNowFullscreen) {
        auto menuBar = wnd->GetGui()->GetMenuBar();
        wnd->SetCursorVisibility(menuBar && menuBar->IsVisible());
    } else {
        wnd->SetCursorVisibility(true);
    }
}
} // namespace Fast