#pragma once

#if defined(ENABLE_DX11) && defined(__WINRT__)

#include <memory>
#include <functional>

#include <dxgi1_2.h>
#include <wrl/client.h>

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "gfx_dxgi_host.h"

namespace Ship {
class Config;
class ConsoleVariable;
class FileDrop;
} // namespace Ship

namespace Fast {
class Fast3dGui;
class GfxRenderingAPIDX11;

class GfxWindowBackendUWP final : public GfxDxgiHost {
  public:
    GfxWindowBackendUWP(std::shared_ptr<Ship::Config> config = nullptr,
                        std::shared_ptr<Ship::FileDrop> fileDrop = nullptr,
                        std::shared_ptr<Ship::ConsoleVariable> consoleVariable = nullptr,
                        std::shared_ptr<Fast::Fast3dGui> fast3dGui = nullptr);
    ~GfxWindowBackendUWP() override;

    void Init(const char* gameName, const char* apiName, bool startFullScreen, uint32_t width, uint32_t height,
              int32_t posX, int32_t posY) override;
    void Close() override;
    void SetKeyboardCallbacks(bool (*onKeyDown)(int scancode), bool (*onKeyUp)(int scancode),
                              void (*onAllKeysUp)()) override;
    void SetMouseCallbacks(bool (*onMouseButtonDown)(int btn), bool (*onMouseButtonUp)(int btn)) override;
    void SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool isNowFullscreen)) override;
    void SetFullscreen(bool fullscreen) override;
    void GetActiveWindowRefreshRate(uint32_t* refreshRate) override;
    void SetCursorVisibility(bool visible) override;
    void SetMousePos(int32_t posX, int32_t posY) override;
    void GetMousePos(int32_t* x, int32_t* y) override;
    void GetMouseDelta(int32_t* x, int32_t* y) override;
    void GetMouseWheel(float* x, float* y) override;
    bool GetMouseState(uint32_t btn) override;
    void SetMouseCapture(bool capture) override;
    bool IsMouseCaptured() override;
    void GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) override;
    void SetDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) override;
    void SetTitle(const char* title);
    Ship::WindowRect GetPrimaryMonitorRect() override;
    void HandleEvents() override;
    bool IsFrameReady() override;
    bool IsWindowVisible() override;
    void SwapBuffersBegin() override;
    void SwapBuffersEnd() override;
    double GetTime() override;
    int GetTargetFps();
    void SetTargetFps(int fps) override;
    void SetMaxFrameLatency(int latency) override;
    const char* GetKeyName(int scancode) override;
    bool CanDisableVsync() override;
    bool IsRunning() override;
    void Destroy() override;
    bool IsFullscreen() override;

    SDL_Window* GetSdlWindow() const {
        return mWnd;
    }

    HWND GetWindowHandle() override;
    IDXGISwapChain1* GetSwapChain() override;
    void CreateSwapChain(IUnknown* device, std::function<void()>&& beforeDestroyFn) override;
    void CreateFactoryAndDevice(bool debug, int d3dVersion, GfxRenderingAPIDX11* self,
                                bool (*createFunc)(GfxRenderingAPIDX11* self, bool softwareRenderer)) override;

  private:
    void HandleSingleEvent(SDL_Event& event);
    int TranslateScancode(int scancode) const;
    void OnKeydown(int scancode) const;
    void OnKeyup(int scancode) const;
    void ApplyMaxFrameLatency();

    IUnknown* GetCoreWindow() const;

    SDL_Window* mWnd = nullptr;
    int mSdlToLusTable[512] = {};
    int mWindowWidth = 1920;
    int mWindowHeight = 1080;
    void (*mOnAllKeysUp)() = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory2> mFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> mSwapChain;
    Microsoft::WRL::ComPtr<IUnknown> mSwapChainDevice;
    std::function<void()> mBeforeDestroySwapChainFn;

    HANDLE mWaitableObject = nullptr;
    uint32_t mMaxFrameLatency = 1;
    uint32_t mAppliedMaxFrameLatency = 0;

    std::shared_ptr<Ship::ConsoleVariable> mConsoleVariable;
    std::shared_ptr<Ship::Config> mConfig;
    std::shared_ptr<Ship::FileDrop> mFileDrop;
};
} // namespace Fast

#ifdef DECLARE_GFX_DXGI_FUNCTIONS
void ThrowIfFailed(HRESULT res);
void ThrowIfFailed(HRESULT res, HWND h_wnd, const char* message);
#endif

#endif
