#pragma once
#if defined(ENABLE_DX11) || defined(ENABLE_DX12)

#include <functional>
#include <memory>

#include <dxgi1_2.h>

#include "gfx_window_manager_api.h"
#include "ship/utils/HResultException.h"

namespace Fast {
class Fast3dGui;
class GfxRenderingAPIDX11;

class GfxDxgiHost : public GfxWindowBackend {
  public:
    ~GfxDxgiHost() override = default;

    virtual IDXGISwapChain1* GetSwapChain() = 0;
    virtual void CreateSwapChain(IUnknown* device, std::function<void()>&& beforeDestroyFn) = 0;
    virtual void CreateFactoryAndDevice(bool debug, int d3dVersion, GfxRenderingAPIDX11* self,
                                        bool (*createFunc)(GfxRenderingAPIDX11* self, bool softwareRenderer)) = 0;

    virtual HWND GetWindowHandle() = 0;

    std::shared_ptr<Fast::Fast3dGui> mFast3dGui;
};
} // namespace Fast

#ifdef DECLARE_GFX_DXGI_FUNCTIONS
void ThrowIfFailed(HRESULT res);
void ThrowIfFailed(HRESULT res, HWND h_wnd, const char* message);
#endif

#endif
