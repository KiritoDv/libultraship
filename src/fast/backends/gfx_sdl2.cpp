#include <stdio.h>

#if defined(ENABLE_OPENGL) || defined(__APPLE__)

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include "ship/config/ConsoleVariable.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/window/FileDrop.h"
#include "fast/backends/gfx_sdl.h"
#include "fast/backends/gfx_sdl_scancodes.h"

#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

#if FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#elif __APPLE__
#include <SDL.h>
#ifdef ENABLE_VULKAN
#include <SDL_vulkan.h>
#endif
#include "fast/backends/gfx_metal.h"
#include "ship/utils/macUtils.h"
#else
#include <SDL2/SDL.h>
#ifdef ENABLE_VULKAN
#include <SDL2/SDL_vulkan.h>
#endif
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengles2.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "ship/window/gui/Gui.h"
#include "fast/Fast3dGui.h"

#ifdef _WIN32
#include <WTypesbase.h>
#include <Windows.h>
#include <SDL_syswm.h>
#endif

#define GFX_BACKEND_NAME "SDL"
#define _100NANOSECONDS_IN_SECOND 10000000

#ifdef _WIN32
LONG_PTR SDL_WndProc;
#endif

namespace Fast {

GfxWindowBackendSDL2::GfxWindowBackendSDL2(std::shared_ptr<Ship::Config> config,
                                           std::shared_ptr<Ship::FileDrop> fileDrop,
                                           std::shared_ptr<Ship::ConsoleVariable> consoleVariable,
                                           std::shared_ptr<Fast::Fast3dGui> fast3dGui)
    : mConsoleVariable(std::move(consoleVariable)), mConfig(std::move(config)), mFileDrop(std::move(fileDrop)),
      mFast3dGui(std::move(fast3dGui)) {
}

GfxWindowBackendSDL2::~GfxWindowBackendSDL2() {
}

void GfxWindowBackendSDL2::SetFullscreenImpl(bool on, bool call_callback) {
    if (mFullScreen == on) {
        return;
    }

    int display_in_use = SDL_GetWindowDisplayIndex(mWnd);
    if (display_in_use < 0) {
        SPDLOG_WARN("Can't detect on which monitor we are. Probably out of display area?");
        SPDLOG_WARN(SDL_GetError());
    }

    if (on) {
        // OTRTODO: Get mode from config.
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(display_in_use, &mode) >= 0) {
            SDL_SetWindowDisplayMode(mWnd, &mode);
        } else {
            SPDLOG_ERROR(SDL_GetError());
        }
    }

#if defined(__APPLE__) && !defined(__IOS__)
    // Implement fullscreening with native macOS APIs
    if (on != isNativeMacOSFullscreenActive(mWnd)) {
        toggleNativeMacOSFullscreen(mWnd);
    }
    mFullScreen = on;
#elif defined(__EMSCRIPTEN__)
    // Browser: ask for fullscreen on the canvas directly, with no resize strategy. SDL's own path
    // assumes the canvas becomes screen-sized, but the browser's fullscreen styling decides the real
    // box (letterboxing, notch, page zoom), which left SDL scaling the mouse against a size the canvas
    // never had. With the default strategy the browser sizes the element, the window resize callback
    // registered in Init() pushes that size into SDL, and input stays 1:1. The request is only honored
    // from inside an input event, so Emscripten defers it to the next one, and the user can leave with
    // Escape; mFullScreen therefore follows the document's fullscreen state in HandleEvents.
    if (on) {
        EMSCRIPTEN_RESULT result = emscripten_request_fullscreen("#canvas", EM_TRUE);
        if (result != EMSCRIPTEN_RESULT_SUCCESS && result != EMSCRIPTEN_RESULT_DEFERRED) {
            SPDLOG_ERROR("Failed to request fullscreen from the browser ({})", (int)result);
        }
    } else {
        emscripten_exit_fullscreen();
    }
#else
    if (SDL_SetWindowFullscreen(mWnd, on ? (mConsoleVariable->GetInteger(CVAR_SDL_WINDOWED_FULLSCREEN, 0)
                                                ? SDL_WINDOW_FULLSCREEN_DESKTOP
                                                : SDL_WINDOW_FULLSCREEN)
                                         : 0) >= 0) {
        mFullScreen = on;
    } else {
        SPDLOG_ERROR("Failed to switch from or to fullscreen mode.");
        SPDLOG_ERROR(SDL_GetError());
    }
#endif

#ifndef __EMSCRIPTEN__ // the canvas goes back to its CSS size on its own (see the resize callback in Init)
    if (!on) {
        mWindowWidth = mConfig->GetInt("Window.Width", 640);
        mWindowHeight = mConfig->GetInt("Window.Height", 480);
        int32_t posX = mConfig->GetInt("Window.PositionX", 100);
        int32_t posY = mConfig->GetInt("Window.PositionY", 100);
        if (display_in_use < 0) { // Fallback to default if out of bounds
            posX = 100;
            posY = 100;
        }
        SDL_SetWindowPosition(mWnd, posX, posY);
        SDL_SetWindowSize(mWnd, mWindowWidth, mWindowHeight);
    }

    if (mOnFullscreenChanged != nullptr && call_callback) {
        mOnFullscreenChanged(on);
    }
#endif
}

void GfxWindowBackendSDL2::GetActiveWindowRefreshRate(uint32_t* refresh_rate) {
    int display_in_use = SDL_GetWindowDisplayIndex(mWnd);

    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(display_in_use, &mode);
    *refresh_rate = mode.refresh_rate != 0 ? mode.refresh_rate : 60;
}

static uint64_t previous_time;
#ifdef _WIN32
static HANDLE mTimer;
#endif

#define FRAME_INTERVAL_US_NUMERATOR 1000000
#define FRAME_INTERVAL_US_DENOMINATOR (mTargetFps)

void GfxWindowBackendSDL2::Close() {
    mIsRunning = false;
}

#ifdef _WIN32
static LRESULT CALLBACK gfx_sdl_wnd_proc(HWND h_wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_GETDPISCALEDSIZE:
            // Something is wrong with SDLs original implementation of WM_GETDPISCALEDSIZE, so pass it to the default
            // system window procedure instead.
            return DefWindowProc(h_wnd, message, w_param, l_param);
        case WM_ENDSESSION: {
            GfxWindowBackendSDL2* self =
                reinterpret_cast<GfxWindowBackendSDL2*>(GetWindowLongPtr(h_wnd, GWLP_USERDATA));
            // Apparently SDL2 does not handle this
            if (w_param == TRUE) {
                self->Close();
            }
            break;
        }
        default:
            // Pass anything else to SDLs original window procedure.
            return CallWindowProc((WNDPROC)SDL_WndProc, h_wnd, message, w_param, l_param);
    }
    return 0;
};
#endif

void GfxWindowBackendSDL2::Init(const char* gameName, const char* gfxApiName, bool startFullScreen, uint32_t width,
                                uint32_t height, int32_t posX, int32_t posY) {
    mWindowWidth = width;
    mWindowHeight = height;

#if defined(__APPLE__) && !defined(__IOS__)
    // Keep running at full speed (and audio playing) while unfocused/hidden by opting
    // out of App Nap, which otherwise throttles the whole process in the background.
    disableMacOSAppNap();
#endif

#if SDL_VERSION_ATLEAST(2, 24, 0)
    /* fix DPI scaling issues on Windows */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif

    SDL_Init(SDL_INIT_VIDEO);

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

#ifdef ENABLE_VULKAN
    bool use_vulkan = strcmp(gfxApiName, "Vulkan") == 0;
#else
    constexpr bool use_vulkan = false;
#endif
#if defined(__APPLE__)
    bool use_opengl = !use_vulkan && strcmp(gfxApiName, "OpenGL") == 0;
#else
    bool use_opengl = !use_vulkan;
#endif

    if (use_opengl) {
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    } else if (!use_vulkan) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    }

#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#elif defined(USE_OPENGLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

#ifdef _WIN32
    // Use high-resolution mTimer by default on Windows 10 (so that NtSetTimerResolution (...) hacks are not needed)
    mTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    // Fallback to low resolution mTimer if unsupported by the OS
    if (mTimer == nullptr) {
        mTimer = CreateWaitableTimer(nullptr, false, nullptr);
    }
#endif

#ifdef __OpenBSD__
    int sysctlname[2] = { CTL_KERN, KERN_CLOCKRATE };
    struct clockinfo clockinfo;
    size_t clockinfo_size = sizeof(struct clockinfo);
    if (sysctl(sysctlname, 2, &clockinfo, &clockinfo_size, NULL, 0) != -1) {
        mBsdTick = clockinfo.tick;
    }
#endif

    char title[512];
    int len = snprintf(title, sizeof(title), "%s (%s)", gameName, gfxApiName);

#if defined(__IOS__) || defined(__ANDROID__)
    Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    if (use_opengl) {
        flags = flags | SDL_WINDOW_OPENGL;
    } else if (use_vulkan) {
        flags = flags | SDL_WINDOW_VULKAN;
    } else {
        flags = flags | SDL_WINDOW_METAL;
    }

#ifdef __EMSCRIPTEN__
    double canvasW = 0.0, canvasH = 0.0;
    emscripten_get_element_css_size("#canvas", &canvasW, &canvasH);
    if (canvasW > 0 && canvasH > 0) {
        mWindowWidth = (int)canvasW;
        mWindowHeight = (int)canvasH;
    }
#endif
    mWnd = SDL_CreateWindow(title, posX, posY, mWindowWidth, mWindowHeight, flags);
#ifdef __EMSCRIPTEN__
    em_ui_callback_func onCanvasResize = [](int, const EmscriptenUiEvent*, void* userData) -> EM_BOOL {
        auto backend = static_cast<GfxWindowBackendSDL2*>(userData);
        double w = 0.0;
        double h = 0.0;
        emscripten_get_element_css_size("#canvas", &w, &h);
        if (w > 0 && h > 0 && backend->mWnd != nullptr) {
            SDL_SetWindowSize(backend->mWnd, (int)w, (int)h);
        }
        return EM_TRUE;
    };
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_FALSE, onCanvasResize);
#endif
#ifdef _WIN32
    // Get Windows window handle and use it to subclass the window procedure.
    // Needed to circumvent SDLs DPI scaling problems under windows (original does only scale *sometimes*).
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(mWnd, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;
    SDL_WndProc = SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)gfx_sdl_wnd_proc);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
#endif
    Fast::GuiWindowInitData window_impl;

    int display_in_use = SDL_GetWindowDisplayIndex(mWnd);
    if (display_in_use < 0) { // Fallback to default if out of bounds
        posX = 100;
        posY = 100;
    }

    if (use_opengl) {
#ifdef __EMSCRIPTEN__
        // Browser: window geometry is CSS points; the canvas backing store is scaled by the device pixel ratio.
        SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
#else
        SDL_GL_GetDrawableSize(mWnd, &mWindowWidth, &mWindowHeight);
#endif

        if (startFullScreen) {
            SetFullscreenImpl(true, false);
        }

        mCtx = SDL_GL_CreateContext(mWnd);

        SDL_GL_MakeCurrent(mWnd, mCtx);
        SDL_GL_SetSwapInterval(mVsyncEnabled ? 1 : 0);

        window_impl.Opengl = { mWnd, mCtx };
    } else if (use_vulkan) {
#ifdef ENABLE_VULKAN
        if (startFullScreen) {
            SetFullscreenImpl(true, false);
        }
        SDL_Vulkan_GetDrawableSize(mWnd, &mWindowWidth, &mWindowHeight);
        window_impl.Vulkan = { mWnd };
#endif
    } else {
        uint32_t flags = SDL_RENDERER_ACCELERATED;
        if (mVsyncEnabled) {
            flags |= SDL_RENDERER_PRESENTVSYNC;
        }
        mRenderer = SDL_CreateRenderer(mWnd, -1, flags);
        if (mRenderer == nullptr) {
            SPDLOG_ERROR("Error creating renderer: {}", SDL_GetError());
            return;
        }

        if (startFullScreen) {
            SetFullscreenImpl(true, false);
        }

#if defined(__IOS__)
        SDL_GetRendererOutputSize(mRenderer, &mWindowWidth, &mWindowHeight);
#else
        // macOS: window geometry is logical points (Metal included); the pixel-density
        // scale is applied where the internal render size is computed.
        SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
#endif
        window_impl.Metal = { mWnd, mRenderer };
    }

    if (mFast3dGui) {
        mFast3dGui->Init(window_impl);
    }

    for (size_t i = 0; i < std::size(lus_to_sdl_table); i++) {
        mSdlToLusTable[lus_to_sdl_table[i]] = i;
    }

    for (size_t i = 0; i < std::size(scancode_rmapping_extended); i++) {
        mSdlToLusTable[scancode_rmapping_extended[i][0]] = mSdlToLusTable[scancode_rmapping_extended[i][1]] + 0x100;
    }

    for (size_t i = 0; i < std::size(scancode_rmapping_nonextended); i++) {
        mSdlToLusTable[scancode_rmapping_nonextended[i][0]] = mSdlToLusTable[scancode_rmapping_nonextended[i][1]];
        mSdlToLusTable[scancode_rmapping_nonextended[i][1]] += 0x100;
    }
}

void GfxWindowBackendSDL2::SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool is_now_fullscreen)) {
    mOnFullscreenChanged = onFullscreenChanged;
}

void GfxWindowBackendSDL2::SetFullscreen(bool enable) {
    SetFullscreenImpl(enable, true);
}

void GfxWindowBackendSDL2::SetCursorVisibility(bool visible) {
    if (visible) {
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        SDL_ShowCursor(SDL_DISABLE);
    }
}

void GfxWindowBackendSDL2::SetMousePos(int32_t x, int32_t y) {
    SDL_WarpMouseInWindow(mWnd, x, y);
}

void GfxWindowBackendSDL2::GetMousePos(int32_t* x, int32_t* y) {
    SDL_GetMouseState(x, y);
}

void GfxWindowBackendSDL2::GetMouseDelta(int32_t* x, int32_t* y) {
    SDL_GetRelativeMouseState(x, y);
}

void GfxWindowBackendSDL2::GetMouseWheel(float* x, float* y) {
    *x = mMouseWheelX;
    *y = mMouseWheelY;
    mMouseWheelX = 0.0f;
    mMouseWheelY = 0.0f;
}

bool GfxWindowBackendSDL2::GetMouseState(uint32_t btn) {
    return SDL_GetMouseState(nullptr, nullptr) & (1 << btn);
}

void GfxWindowBackendSDL2::SetMouseCapture(bool capture) {
    SDL_SetRelativeMouseMode(static_cast<SDL_bool>(capture));
    // TODO: Manually setting a clipping rect here because
    // https://wiki.libsdl.org/SDL2/SDL_HINT_MOUSE_RELATIVE_MODE_CENTER isn't working as epxected.
    // Revisit on SDL3
    auto mouse = SDL_GetWindowMouseRect(mWnd);
    if (capture) {
        int w, h;
        SDL_GetWindowSize(mWnd, &w, &h);
        mCursorClip = { (w / 2) - 1, (h / 2) - 1, 2, 2 };
    }
    SDL_SetWindowMouseRect(mWnd, capture ? &mCursorClip : NULL);
}

bool GfxWindowBackendSDL2::IsMouseCaptured() {
    return (SDL_GetRelativeMouseMode() == SDL_TRUE);
}

void GfxWindowBackendSDL2::SetKeyboardCallbacks(bool (*onKeyDown)(int scancode), bool (*onKeyUp)(int scancode),
                                                void (*onAllKeysUp)()) {
    mOnKeyDown = onKeyDown;
    mOnKeyUp = onKeyUp;
    mOnAllKeysUp = onAllKeysUp;
}

void GfxWindowBackendSDL2::SetMouseCallbacks(bool (*onMouseButtonDown)(int btn), bool (*onMouseButtonUp)(int btn)) {
    mOnMouseButtonDown = onMouseButtonDown;
    mOnMouseButtonUp = onMouseButtonUp;
}

void GfxWindowBackendSDL2::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
#if defined(__APPLE__) && defined(__IOS__)
    if (mRenderer != nullptr) {
        // Metal: drawable pixels, not window points.
        SDL_GetRendererOutputSize(mRenderer, static_cast<int*>((void*)width), static_cast<int*>((void*)height));
    } else {
        SDL_GetWindowSize(mWnd, static_cast<int*>((void*)width), static_cast<int*>((void*)height));
    }
#elif defined(__APPLE__) || defined(__EMSCRIPTEN__)
    // macOS and browsers: window geometry is logical points (CSS pixels on the web); the
    // pixel-density scale is applied where the internal render size is computed.
    SDL_GetWindowSize(mWnd, static_cast<int*>((void*)width), static_cast<int*>((void*)height));
#else
    SDL_GL_GetDrawableSize(mWnd, static_cast<int*>((void*)width), static_cast<int*>((void*)height));
#endif
    SDL_GetWindowPosition(mWnd, static_cast<int*>(posX), static_cast<int*>(posY));
}

void GfxWindowBackendSDL2::SetDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) {
    mWindowWidth = width;
    mWindowHeight = height;
    if (mWnd) {
        SDL_SetWindowPosition(mWnd, posX, posY);
        SDL_SetWindowSize(mWnd, mWindowWidth, mWindowHeight);
    }
}

Ship::WindowRect GfxWindowBackendSDL2::GetPrimaryMonitorRect() {
    SDL_DisplayMode mode;
    int display_in_use = mWnd ? SDL_GetWindowDisplayIndex(mWnd) : 0;
    if (display_in_use < 0) {
        SPDLOG_WARN("Can't detect on which monitor we are. Probably out of display area? ({})", SDL_GetError());
        display_in_use = 0;
    }
    if (SDL_GetDesktopDisplayMode(display_in_use, &mode) >= 0) {
        return { 0, 0, mode.w, mode.h };
    }
    SPDLOG_ERROR("Failed to get SDL Desktop Display Mode: ({})", SDL_GetError());
    return { 0, 0, 0, 0 };
}

int GfxWindowBackendSDL2::TranslateScancode(int scancode) const {
    if (scancode < 512) {
        return mSdlToLusTable[scancode];
    }
    return 0;
}

int GfxWindowBackendSDL2::UntranslateScancode(int translatedScancode) const {
    for (int i = 0; i < 512; i++) {
        if (mSdlToLusTable[i] == translatedScancode) {
            return i;
        }
    }

    return 0;
}

void GfxWindowBackendSDL2::OnKeydown(int scancode) const {
    int key = TranslateScancode(scancode);
    if (mOnKeyDown != nullptr) {
        mOnKeyDown(key);
    }
}

void GfxWindowBackendSDL2::OnKeyup(int scancode) const {
    int key = TranslateScancode(scancode);
    if (mOnKeyUp != nullptr) {
        mOnKeyUp(key);
    }
}

void GfxWindowBackendSDL2::OnMouseButtonDown(int btn) const {
    if (!(btn >= 0 && btn < 5)) {
        return;
    }
    if (mOnMouseButtonDown != nullptr) {
        mOnMouseButtonDown(btn);
    }
}

void GfxWindowBackendSDL2::OnMouseButtonUp(int btn) const {
    if (mOnMouseButtonUp != nullptr) {
        mOnMouseButtonUp(btn);
    }
}

void GfxWindowBackendSDL2::HandleSingleEvent(SDL_Event& event) {
    Fast::WindowEvent event_impl;
    event_impl.Sdl = { &event };
    if (mFast3dGui) {
        mFast3dGui->HandleWindowEvents(event_impl);
    } else {
        SPDLOG_ERROR("gfx_sdl2: Gui is not a Fast3dGui; cannot dispatch window event");
    }
    switch (event.type) {
#ifndef TARGET_WEB
        // Scancodes are broken in Emscripten SDL2: https://bugzilla.libsdl.org/show_bug.cgi?id=3259
        case SDL_KEYDOWN:
            OnKeydown(event.key.keysym.scancode);
            break;
        case SDL_KEYUP:
            OnKeyup(event.key.keysym.scancode);
            break;
        case SDL_MOUSEBUTTONDOWN:
            OnMouseButtonDown(event.button.button - 1);
            break;
        case SDL_MOUSEBUTTONUP:
            OnMouseButtonUp(event.button.button - 1);
            break;
        case SDL_MOUSEWHEEL:
            mMouseWheelX = event.wheel.x;
            mMouseWheelY = event.wheel.y;
            break;
#endif
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
#if defined(__APPLE__) && defined(__IOS__)
                    if (mRenderer != nullptr) {
                        // Metal: drawable pixels, not window points.
                        SDL_GetRendererOutputSize(mRenderer, &mWindowWidth, &mWindowHeight);
                    } else {
                        SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
                    }
#elif defined(__APPLE__) || defined(__EMSCRIPTEN__)
                    // macOS and browsers: window geometry is logical points (CSS pixels on the web); the
                    // pixel-density scale is applied where the internal render size is computed.
                    SDL_GetWindowSize(mWnd, &mWindowWidth, &mWindowHeight);
#else
                    SDL_GL_GetDrawableSize(mWnd, &mWindowWidth, &mWindowHeight);
#endif
                    break;
                case SDL_WINDOWEVENT_CLOSE:
                    if (event.window.windowID == SDL_GetWindowID(mWnd)) {
                        // We listen specifically for main window close because closing main window
                        // on macOS does not trigger SDL_Quit.
                        Close();
                    }
                    break;
            }
            break;
        case SDL_DROPFILE:
            mFileDrop->SetDroppedFile(event.drop.file);
            break;
        case SDL_QUIT:
            Close();
            break;
    }
}

void GfxWindowBackendSDL2::HandleEvents() {
    SDL_Event event;
    SDL_PumpEvents();
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_CONTROLLERDEVICEADDED - 1) > 0) {
        HandleSingleEvent(event);
    }
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEREMOVED + 1, SDL_LASTEVENT) > 0) {
        HandleSingleEvent(event);
    }

    // resync fullscreen state
#if defined(__APPLE__) && !defined(__IOS__)
    auto nextFullscreenState = isNativeMacOSFullscreenActive(mWnd);
    if (mFullScreen != nextFullscreenState) {
        mFullScreen = nextFullscreenState;
        if (mOnFullscreenChanged != nullptr) {
            mOnFullscreenChanged(mFullScreen);
        }
    }
#elif defined(__EMSCRIPTEN__)
    // The browser enters fullscreen on the next input event after the request and can leave it on
    // its own (Escape), so follow the document's state rather than the request.
    EmscriptenFullscreenChangeEvent fullscreenStatus;
    const bool nextFullscreenState = emscripten_get_fullscreen_status(&fullscreenStatus) == EMSCRIPTEN_RESULT_SUCCESS &&
                                     fullscreenStatus.isFullscreen;
    if (mFullScreen != nextFullscreenState) {
        mFullScreen = nextFullscreenState;
        if (mOnFullscreenChanged != nullptr) {
            mOnFullscreenChanged(mFullScreen);
        }
    }
#endif
}

bool GfxWindowBackendSDL2::IsFrameReady() {
    return true;
}

bool GfxWindowBackendSDL2::IsWindowVisible() {
    const uint32_t flags = SDL_GetWindowFlags(mWnd);
    if (flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
        return false;
    }
#if defined(__APPLE__) && !defined(__IOS__)
    // SDL2 has no occlusion flag; query Cocoa directly. A fully-covered window stops
    // being composited, which stalls Metal's drawable acquisition (see isWindowOccluded).
    if (isWindowOccluded(mWnd)) {
        return false;
    }
#endif
    return true;
}

static uint64_t qpc_to_100ns(uint64_t qpc) {
    const uint64_t qpc_freq = SDL_GetPerformanceFrequency();
    return qpc / qpc_freq * _100NANOSECONDS_IN_SECOND + qpc % qpc_freq * _100NANOSECONDS_IN_SECOND / qpc_freq;
}

void GfxWindowBackendSDL2::SyncFramerateWithTime() const {
    uint64_t t = qpc_to_100ns(SDL_GetPerformanceCounter());

    const int64_t next = previous_time + 10 * FRAME_INTERVAL_US_NUMERATOR / FRAME_INTERVAL_US_DENOMINATOR;
    int64_t left = next - t;
#ifdef _WIN32
    // We want to exit a bit early, so we can busy-wait the rest to never miss the deadline
    left -= 15000UL;
#elif defined(__APPLE__)
    // Use macOS scheduler interval on macOS. Don't trust sysctl on macOS
    left -= 10000UL;
#elif defined(__OpenBSD__)
    left -= mBsdTick * 10;
#endif
    if (left > 0) {
#ifndef _WIN32
        const timespec spec = { 0, left * 100 };
        nanosleep(&spec, nullptr);
#else
        // The accuracy of this mTimer seems to usually be within +- 1.0 ms
        LARGE_INTEGER li;
        li.QuadPart = -left;
        SetWaitableTimer(mTimer, &li, 0, nullptr, nullptr, false);
        WaitForSingleObject(mTimer, INFINITE);
#endif
    }

    t = qpc_to_100ns(SDL_GetPerformanceCounter());
#ifdef _WIN32
    while (t < next) {
        YieldProcessor(); // TODO: Find a way for other compilers, OSes and architectures
        t = qpc_to_100ns(SDL_GetPerformanceCounter());
    }
#endif
    if (left > 0 && t - next < 10000) {
        // In case it takes some time for the application to wake up after sleep,
        // or inaccurate mTimer,
        // don't let that slow down the framerate.
        t = next;
    }
    previous_time = t;
}

void GfxWindowBackendSDL2::SwapBuffersBegin() {
    bool nextVsyncEnabled = mConsoleVariable->GetInteger(CVAR_VSYNC_ENABLED, 1);

    if (mVsyncEnabled != nextVsyncEnabled) {
        mVsyncEnabled = nextVsyncEnabled;
        if (mCtx != nullptr) {
            SDL_GL_SetSwapInterval(mVsyncEnabled ? 1 : 0);
        }
        if (mRenderer != nullptr) {
            SDL_RenderSetVSync(mRenderer, mVsyncEnabled ? 1 : 0);
        }
    }

    SyncFramerateWithTime();
    SDL_GL_SwapWindow(mWnd);
}

void GfxWindowBackendSDL2::SwapBuffersEnd() {
}

double GfxWindowBackendSDL2::GetTime() {
    return 0.0;
}

int GfxWindowBackendSDL2::GetTargetFps() {
    return mTargetFps;
}

void GfxWindowBackendSDL2::SetTargetFps(int fps) {
    mTargetFps = fps;
}

void GfxWindowBackendSDL2::SetMaxFrameLatency(int latency) {
    // Not supported by SDL :(
}

const char* GfxWindowBackendSDL2::GetKeyName(int scancode) {
    return SDL_GetScancodeName((SDL_Scancode)UntranslateScancode(scancode));
}

bool GfxWindowBackendSDL2::CanDisableVsync() {
    return true;
}

bool GfxWindowBackendSDL2::IsRunning() {
    return mIsRunning;
}

void GfxWindowBackendSDL2::Destroy() {
    // TODO: destroy _any_ resources used by SDL
    if (mRenderer != nullptr) {
        SDL_DestroyRenderer(mRenderer);
    }
    if (mCtx != nullptr) {
        SDL_GL_DeleteContext(mCtx);
    }
    SDL_DestroyWindow(mWnd);
    SDL_Quit();
}

bool GfxWindowBackendSDL2::IsFullscreen() {
    return mFullScreen;
}
} // namespace Fast
#endif
