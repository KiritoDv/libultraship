#pragma once
#include <memory>
#include "window/Window.h"
#include "engine/libultra/UltraContext.h"

namespace Fast {
class RT64Window : public Ship::Window {
  public:
    RT64Window();
    RT64Window(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows, std::shared_ptr<LUS::SDKContext> ultraContext);
    ~RT64Window();

    void Init() override;
    void Close() override;
    void StartFrame() override;
    void EndFrame() override;
    void SetCursorVisibility(bool visible) override;
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    int32_t GetPosX() override;
    int32_t GetPosY() override;
    uint32_t GetCurrentRefreshRate() override;
    bool SupportsWindowedFullscreen() override;
    bool CanDisableVerticalSync() override;
    void SetResolutionMultiplier(float multiplier) override;
    void SetMsaaLevel(uint32_t value) override;
    void SetFullscreen(bool isFullscreen) override;
    bool IsFullscreen() override;
    bool IsRunning() override;
    const char* GetKeyName(int32_t scancode) override;

    void InitWindowManager();
    void SetTargetFps(int32_t fps);
    void SetMaximumFrameLatency(int32_t latency);
    void GetPixelDepthPrepare(float x, float y);
    uint16_t GetPixelDepth(float x, float y);
    void SetTextureFilter(FilteringMode filteringMode);

  protected:
    static bool KeyDown(int32_t scancode);
    static bool KeyUp(int32_t scancode);
    static void AllKeysUp(void);
    static void OnFullscreenChanged(bool isNowFullscreen);

  private:
    std::shared_ptr<Ship::UltraContext> mUltraContext;
  // private:
  //   GfxRenderingAPI* mRenderingApi;
  //   GfxWindowManagerAPI* mWindowManagerApi;
};
} // namespace Fast
