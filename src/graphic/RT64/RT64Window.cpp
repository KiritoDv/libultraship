#include "RT64Window.h"

#include "Context.h"
#include "public/bridge/consolevariablebridge.h"
#include "graphic/Fast3D/gfx_rendering_api.h"

#define HLSL_CPU
#include "hle/rt64_application.h"
#include "rt64_render_hooks.h"

#include <fstream>

namespace Fast {
RT64Window::RT64Window() : RT64Window(std::vector<std::shared_ptr<Ship::GuiWindow>>(), std::static_pointer_cast<LUS::SDKContext>(std::make_shared<Ship::UltraContext>())) {
}

RT64Window::RT64Window(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows, std::shared_ptr<LUS::SDKContext> ultraContext) : Ship::Window(guiWindows, ultraContext) {

}

RT64Window::~RT64Window() {
    SPDLOG_DEBUG("destruct RT64Window");
}

void RT64Window::Init() {
    RT64::Application::Core appCore{};
    this->mUltraContext->Create();
#if defined(_WIN32)
    // appCore.window = window_handle.window;
#elif defined(__linux__)
    appCore.window.display = window_handle.display;
    appCore.window.window = window_handle.window;
#endif

    appCore.RDRAM = this->mUltraContext->mem->DRAM;
    appCore.DMEM = this->mUltraContext->mem->DMEM;
    appCore.IMEM = this->mUltraContext->mem->IMEM;

    appCore.MI_INTR_REG = &this->mUltraContext->mem->INTR_REG;

    appCore.DPC_START_REG    = &this->mUltraContext->dpc->START_REG;
    appCore.DPC_END_REG      = &this->mUltraContext->dpc->END_REG;
    appCore.DPC_CURRENT_REG  = &this->mUltraContext->dpc->CURRENT_REG;
    appCore.DPC_STATUS_REG   = &this->mUltraContext->dpc->STATUS_REG;
    appCore.DPC_CLOCK_REG    = &this->mUltraContext->dpc->CLOCK_REG;
    appCore.DPC_BUFBUSY_REG  = &this->mUltraContext->dpc->BUFBUSY_REG;
    appCore.DPC_PIPEBUSY_REG = &this->mUltraContext->dpc->PIPEBUSY_REG;
    appCore.DPC_TMEM_REG     = &this->mUltraContext->dpc->TMEM_REG;

    appCore.VI_STATUS_REG = &this->mUltraContext->vi->STATUS_REG;
    appCore.VI_ORIGIN_REG = &this->mUltraContext->vi->ORIGIN_REG;
    appCore.VI_WIDTH_REG  = &this->mUltraContext->vi->WIDTH_REG;
    appCore.VI_INTR_REG   = &this->mUltraContext->vi->INTR_REG;
    appCore.VI_V_CURRENT_LINE_REG = &this->mUltraContext->vi->V_CURRENT_LINE_REG;
    appCore.VI_TIMING_REG = &this->mUltraContext->vi->TIMING_REG;
    appCore.VI_V_SYNC_REG = &this->mUltraContext->vi->V_SYNC_REG;
    appCore.VI_H_SYNC_REG = &this->mUltraContext->vi->H_SYNC_REG;
    appCore.VI_LEAP_REG   = &this->mUltraContext->vi->LEAP_REG;
    appCore.VI_H_START_REG = &this->mUltraContext->vi->H_START_REG;
    appCore.VI_V_START_REG = &this->mUltraContext->vi->V_START_REG;
    appCore.VI_V_BURST_REG = &this->mUltraContext->vi->V_BURST_REG;
    appCore.VI_X_SCALE_REG = &this->mUltraContext->vi->X_SCALE_REG;
    appCore.VI_Y_SCALE_REG = &this->mUltraContext->vi->Y_SCALE_REG;
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