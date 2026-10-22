#if defined(ENABLE_DX11) && defined(__WINRT__)

#include <iterator> // std::size, for the scancode tables
#include <utility>

#include <windows.h>
#include <inspectable.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#if __has_include(<SDL2/SDL_syswm.h>)
#include <SDL2/SDL_syswm.h>
#else
#include <SDL_syswm.h>
#endif

#define DECLARE_GFX_DXGI_FUNCTIONS
#include "fast/backends/gfx_uwp.h"
#include "fast/backends/gfx_sdl_scancodes.h"
#include "fast/backends/gfx_screen_config.h"
#include "fast/Fast3dGui.h"
#include "fast/WindowEvent.h"
#include "ship/config/Config.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/HResultException.h"
#include "ship/window/FileDrop.h"
#include "ship/window/gui/Gui.h"

#include "spdlog/spdlog.h"

using namespace Microsoft::WRL;

#define _100NANOSECONDS_IN_SECOND 10000000

namespace Fast {

GfxWindowBackendUWP::GfxWindowBackendUWP(std::shared_ptr<Ship::Config> config, std::shared_ptr<Ship::FileDrop> fileDrop,
                                         std::shared_ptr<Ship::ConsoleVariable> consoleVariable,
                                         std::shared_ptr<Fast::Fast3dGui> fast3dGui)
    : mConsoleVariable(std::move(consoleVariable)), mConfig(std::move(config)), mFileDrop(std::move(fileDrop)) {
    mFast3dGui = std::move(fast3dGui);
}

GfxWindowBackendUWP::~GfxWindowBackendUWP() {
    if (mWnd != nullptr) {
        SDL_DestroyWindow(mWnd);
        mWnd = nullptr;
    }
}

void GfxWindowBackendUWP::Init(const char* gameName, const char* apiName, bool startFullScreen, uint32_t width,
                               uint32_t height, int32_t posX, int32_t posY) {
    (void)startFullScreen;
    (void)posX;
    (void)posY;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        SPDLOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    char title[512];
    snprintf(title, sizeof(title), "%s (%s)", gameName, apiName);

    mWnd = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, static_cast<int>(width),
                            static_cast<int>(height), SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (mWnd == nullptr) {
        SPDLOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
        return;
    }

    SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
    mFullScreen = true;

    for (size_t i = 0; i < std::size(lus_to_sdl_table); i++) {
        mSdlToLusTable[lus_to_sdl_table[i]] = static_cast<int>(i);
    }
    for (size_t i = 0; i < std::size(scancode_rmapping_extended); i++) {
        mSdlToLusTable[scancode_rmapping_extended[i][0]] = mSdlToLusTable[scancode_rmapping_extended[i][1]] + 0x100;
    }
    for (size_t i = 0; i < std::size(scancode_rmapping_nonextended); i++) {
        mSdlToLusTable[scancode_rmapping_nonextended[i][0]] = mSdlToLusTable[scancode_rmapping_nonextended[i][1]];
        mSdlToLusTable[scancode_rmapping_nonextended[i][1]] += 0x100;
    }

}

IUnknown* GfxWindowBackendUWP::GetCoreWindow() const {
    if (mWnd == nullptr) {
        return nullptr;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(mWnd, &info) != SDL_TRUE) {
        SPDLOG_ERROR("SDL_GetWindowWMInfo failed: {}", SDL_GetError());
        return nullptr;
    }
    if (info.subsystem != SDL_SYSWM_WINRT) {
        SPDLOG_ERROR("Expected the SDL WinRT video driver, got subsystem {}", static_cast<int>(info.subsystem));
        return nullptr;
    }
    return reinterpret_cast<IUnknown*>(info.info.winrt.window);
}

void GfxWindowBackendUWP::CreateFactoryAndDevice(bool debug, int d3dVersion, GfxRenderingAPIDX11* self,
                                                 bool (*createFunc)(GfxRenderingAPIDX11* self, bool softwareRenderer)) {
    (void)d3dVersion;

    ThrowIfFailed(CreateDXGIFactory2(debug ? DXGI_CREATE_FACTORY_DEBUG : 0, __uuidof(IDXGIFactory2), &mFactory));

    if (!createFunc(self, false)) {
        SPDLOG_CRITICAL("Creating the D3D11 device failed.");
        throw Ship::HResultException(E_FAIL, "Creating the D3D11 device failed");
    }
}

void GfxWindowBackendUWP::CreateSwapChain(IUnknown* device, std::function<void()>&& beforeDestroyFn) {
    IUnknown* coreWindow = GetCoreWindow();
    if (coreWindow == nullptr) {
        throw Ship::HResultException(E_FAIL, "No CoreWindow to attach a swapchain to");
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = static_cast<UINT>(mWindowWidth);
    swapChainDesc.Height = static_cast<UINT>(mWindowHeight);
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 3;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ThrowIfFailed(mFactory->CreateSwapChainForCoreWindow(device, coreWindow, &swapChainDesc, nullptr, &mSwapChain));

    ApplyMaxFrameLatency();
    if (mWaitableObject != nullptr) {
        WaitForSingleObject(mWaitableObject, INFINITE);
    }

    mSwapChainDevice = device;
    mBeforeDestroySwapChainFn = std::move(beforeDestroyFn);
}

void GfxWindowBackendUWP::ApplyMaxFrameLatency() {
    ComPtr<IDXGISwapChain2> swapChain2;
    if (mSwapChain->QueryInterface(__uuidof(IDXGISwapChain2), &swapChain2) == S_OK) {
        ThrowIfFailed(swapChain2->SetMaximumFrameLatency(mMaxFrameLatency));
        if (mWaitableObject == nullptr) {
            mWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
        }
    } else {
        ComPtr<IDXGIDevice1> device1;
        ThrowIfFailed(mSwapChain->GetDevice(__uuidof(IDXGIDevice1), &device1));
        ThrowIfFailed(device1->SetMaximumFrameLatency(mMaxFrameLatency));
    }
    mAppliedMaxFrameLatency = mMaxFrameLatency;
}

HWND GfxWindowBackendUWP::GetWindowHandle() {
    return nullptr;
}

IDXGISwapChain1* GfxWindowBackendUWP::GetSwapChain() {
    return mSwapChain.Get();
}

void GfxWindowBackendUWP::Close() {
    mIsRunning = false;
}

void GfxWindowBackendUWP::SetKeyboardCallbacks(bool (*onKeyDown)(int scancode), bool (*onKeyUp)(int scancode),
                                               void (*onAllKeysUp)()) {
    mOnKeyDown = onKeyDown;
    mOnKeyUp = onKeyUp;
    mOnAllKeysUp = onAllKeysUp;
}

void GfxWindowBackendUWP::SetMouseCallbacks(bool (*onMouseButtonDown)(int btn), bool (*onMouseButtonUp)(int btn)) {
    mOnMouseButtonDown = onMouseButtonDown;
    mOnMouseButtonUp = onMouseButtonUp;
}

void GfxWindowBackendUWP::SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool isNowFullscreen)) {
    mOnFullscreenChanged = onFullscreenChanged;
}

void GfxWindowBackendUWP::SetFullscreen(bool fullscreen) {
    (void)fullscreen;
    if (mOnFullscreenChanged != nullptr) {
        mOnFullscreenChanged(true);
    }
}

bool GfxWindowBackendUWP::IsFullscreen() {
    return true;
}

void GfxWindowBackendUWP::GetActiveWindowRefreshRate(uint32_t* refreshRate) {
    SDL_DisplayMode mode;
    if (mWnd != nullptr && SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(mWnd), &mode) == 0 &&
        mode.refresh_rate > 0) {
        *refreshRate = static_cast<uint32_t>(mode.refresh_rate);
        return;
    }
    *refreshRate = 60;
}

void GfxWindowBackendUWP::SetCursorVisibility(bool visible) {
    (void)visible;
}

void GfxWindowBackendUWP::SetMousePos(int32_t posX, int32_t posY) {
    (void)posX;
    (void)posY;
}

void GfxWindowBackendUWP::GetMousePos(int32_t* x, int32_t* y) {
    *x = 0;
    *y = 0;
}

void GfxWindowBackendUWP::GetMouseDelta(int32_t* x, int32_t* y) {
    *x = 0;
    *y = 0;
}

void GfxWindowBackendUWP::GetMouseWheel(float* x, float* y) {
    *x = 0.0f;
    *y = 0.0f;
}

bool GfxWindowBackendUWP::GetMouseState(uint32_t btn) {
    (void)btn;
    return false;
}

void GfxWindowBackendUWP::SetMouseCapture(bool capture) {
    (void)capture;
}

bool GfxWindowBackendUWP::IsMouseCaptured() {
    return false;
}

void GfxWindowBackendUWP::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    *width = static_cast<uint32_t>(mWindowWidth);
    *height = static_cast<uint32_t>(mWindowHeight);
    *posX = 0;
    *posY = 0;
}

void GfxWindowBackendUWP::SetDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) {
    (void)width;
    (void)height;
    (void)posX;
    (void)posY;
}

void GfxWindowBackendUWP::SetTitle(const char* title) {
    if (mWnd != nullptr) {
        SDL_SetWindowTitle(mWnd, title);
    }
}

Ship::WindowRect GfxWindowBackendUWP::GetPrimaryMonitorRect() {
    return { 0, 0, mWindowWidth, mWindowHeight };
}

int GfxWindowBackendUWP::TranslateScancode(int scancode) const {
    if (scancode < 512) {
        return mSdlToLusTable[scancode];
    }
    return 0;
}

void GfxWindowBackendUWP::OnKeydown(int scancode) const {
    const int key = TranslateScancode(scancode);
    if (mOnKeyDown != nullptr) {
        mOnKeyDown(key);
    }
}

void GfxWindowBackendUWP::OnKeyup(int scancode) const {
    const int key = TranslateScancode(scancode);
    if (mOnKeyUp != nullptr) {
        mOnKeyUp(key);
    }
}

void GfxWindowBackendUWP::HandleSingleEvent(SDL_Event& event) {
    Fast::WindowEvent eventImpl;
    eventImpl.Sdl = { &event };
    if (mFast3dGui) {
        mFast3dGui->HandleWindowEvents(eventImpl);
    }

    switch (event.type) {
        case SDL_KEYDOWN:
            OnKeydown(event.key.keysym.scancode);
            break;
        case SDL_KEYUP:
            OnKeyup(event.key.keysym.scancode);
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    if (mOnAllKeysUp != nullptr) {
                        mOnAllKeysUp();
                    }
                    break;
                default:
                    break;
            }
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            if (mOnAllKeysUp != nullptr) {
                mOnAllKeysUp();
            }
            break;
        case SDL_QUIT:
            Close();
            break;
        default:
            break;
    }
}

void GfxWindowBackendUWP::HandleEvents() {
    SDL_Event event;
    SDL_PumpEvents();
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_CONTROLLERDEVICEADDED - 1) > 0) {
        HandleSingleEvent(event);
    }
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEREMOVED + 1, SDL_LASTEVENT) > 0) {
        HandleSingleEvent(event);
    }
}

bool GfxWindowBackendUWP::IsFrameReady() {
    return true;
}

bool GfxWindowBackendUWP::IsWindowVisible() {
    if (mWnd == nullptr) {
        return false;
    }
    const uint32_t flags = SDL_GetWindowFlags(mWnd);
    return (flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) == 0;
}

void GfxWindowBackendUWP::SwapBuffersBegin() {
    if (mSwapChain == nullptr) {
        return;
    }

    mVsyncEnabled = mConsoleVariable == nullptr || mConsoleVariable->GetInteger(CVAR_VSYNC_ENABLED, 1) != 0;

    ThrowIfFailed(mSwapChain->Present(mVsyncEnabled ? 1 : 0, 0));
}

void GfxWindowBackendUWP::SwapBuffersEnd() {
    if (mSwapChain == nullptr) {
        return;
    }
    if (mAppliedMaxFrameLatency != mMaxFrameLatency) {
        ApplyMaxFrameLatency();
    }
    if (mWaitableObject != nullptr) {
        WaitForSingleObject(mWaitableObject, INFINITE);
    }
}

double GfxWindowBackendUWP::GetTime() {
    return static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
}

int GfxWindowBackendUWP::GetTargetFps() {
    return mTargetFps;
}

void GfxWindowBackendUWP::SetTargetFps(int fps) {
    mTargetFps = fps;
}

void GfxWindowBackendUWP::SetMaxFrameLatency(int latency) {
    mMaxFrameLatency = static_cast<uint32_t>(latency);
}

const char* GfxWindowBackendUWP::GetKeyName(int scancode) {
    return SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
}

bool GfxWindowBackendUWP::CanDisableVsync() {
    return false;
}

bool GfxWindowBackendUWP::IsRunning() {
    return mIsRunning;
}

void GfxWindowBackendUWP::Destroy() {
    mBeforeDestroySwapChainFn = nullptr;
    mSwapChain.Reset();
    mSwapChainDevice.Reset();
    mFactory.Reset();
    mWaitableObject = nullptr;
}

} // namespace Fast

void ThrowIfFailed(HRESULT res) {
    if (FAILED(res)) {
        SPDLOG_ERROR("D3D call failed: 0x{:08X}", static_cast<uint32_t>(res));
        throw Ship::HResultException(res);
    }
}

void ThrowIfFailed(HRESULT res, HWND h_wnd, const char* message) {
    (void)h_wnd;
    if (FAILED(res)) {
        SPDLOG_ERROR("{} (HRESULT 0x{:08X})", message, static_cast<uint32_t>(res));
        throw Ship::HResultException(res, message);
    }
}

#endif
