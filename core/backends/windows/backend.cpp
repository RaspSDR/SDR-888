// Windows backend implementation using Win32 + DirectX11, inspired by windows_example.cpp
#include <backend.h>
#include "imgui.h"
#include "implot.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <string>
#include <filesystem>

#include <utils/flog.h>
#include <version.h>
#include <core.h>
#include <gui/gui.h>

// Data (mostly adapted from the Dear ImGui DX11 Win32 example)
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND g_hWnd = nullptr;
static WNDCLASSEXW g_wc = { 0 };

// Window state
static bool maximized = false;
static bool fullScreen = false;
static int winWidth = 1280;
static int winHeight = 800;
static bool _maximized = false;
static int _winWidth = 0, _winHeight = 0;
static RECT windowedRect = { 100, 100, 100 + 1280, 100 + 800 };
static DWORD windowedStyle = WS_OVERLAPPEDWINDOW;
static DWORD windowedExStyle = 0;

// Forward declarations
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace backend {
    static std::wstring to_wstring(const std::string& s) {
        if (s.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return std::wstring();
        std::wstring ws;
        ws.resize(len - 1);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
        return ws;
    }
    static void applyWindowedSizeFromConfig() {
        core::configManager.acquire();
        int cfgW = core::configManager.conf["windowSize"]["w"];
        int cfgH = core::configManager.conf["windowSize"]["h"];
        core::configManager.release();
        if (cfgW > 0 && cfgH > 0) {
            winWidth = cfgW;
            winHeight = cfgH;
        }
    }

    static void toggleFullscreen(bool enable) {
        if (!g_hWnd) return;
        if (enable == fullScreen) return;

        if (enable) {
            windowedStyle = GetWindowLong(g_hWnd, GWL_STYLE);
            windowedExStyle = GetWindowLong(g_hWnd, GWL_EXSTYLE);
            GetWindowRect(g_hWnd, &windowedRect);

            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                SetWindowLong(g_hWnd, GWL_STYLE, windowedStyle & ~(WS_OVERLAPPEDWINDOW));
                SetWindowLong(g_hWnd, GWL_EXSTYLE, windowedExStyle);
                SetWindowPos(g_hWnd, HWND_TOP,
                             mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
            fullScreen = true;
        }
        else {
            SetWindowLong(g_hWnd, GWL_STYLE, windowedStyle);
            SetWindowLong(g_hWnd, GWL_EXSTYLE, windowedExStyle);
            SetWindowPos(g_hWnd, NULL,
                         windowedRect.left, windowedRect.top,
                         windowedRect.right - windowedRect.left,
                         windowedRect.bottom - windowedRect.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullScreen = false;
        }
    }

    int init(std::string resDir) {
        // Load config
        core::configManager.acquire();
        winWidth = core::configManager.conf["windowSize"]["w"];
        winHeight = core::configManager.conf["windowSize"]["h"];
        maximized = core::configManager.conf["maximized"];
        fullScreen = core::configManager.conf["fullscreen"];
        core::configManager.release();
        if (winWidth <= 0) winWidth = 1280;
        if (winHeight <= 0) winHeight = 800;
        _maximized = maximized;

        // Create application window
        ImGui_ImplWin32_EnableDpiAwareness();
        const wchar_t* class_name = L"SDR++ Win32 Class";
        g_wc = { sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, class_name, nullptr };

        // load icon
        std::string root = (std::string)core::args["root"];
        root += "/res/icons/sdr888.ico";
        g_wc.hIconSm = g_wc.hIcon = (HICON)LoadImageA(
            nullptr,
            root.c_str(),
            IMAGE_ICON,
            32, 32,
            LR_LOADFROMFILE | LR_DEFAULTSIZE
        );

        if (!::RegisterClassExW(&g_wc)) {
            flog::error("Failed to register window class");
            return 1;
        }

        int posX = 100, posY = 100;
        RECT wr = { posX, posY, posX + winWidth, posY + winHeight };
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

        std::string titleA = std::string("SDR++ v") + VERSION_STR + " (Built at " __TIME__ ", " __DATE__ ")";
        std::wstring title = to_wstring(titleA);

        g_hWnd = ::CreateWindowW(g_wc.lpszClassName, title.empty() ? L"SDR++" : title.c_str(), WS_OVERLAPPEDWINDOW, posX, posY,
                                 wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, g_wc.hInstance, nullptr);
        if (!g_hWnd) {
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
            flog::error("Failed to create window");
            return 1;
        }

        // Initialize Direct3D
        if (!CreateDeviceD3D(g_hWnd)) {
            CleanupDeviceD3D();
            ::DestroyWindow(g_hWnd);
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
            return 1;
        }

        if (maximized) {
            ShowWindow(g_hWnd, SW_SHOWMAXIMIZED);
        }
        else {
            ::ShowWindow(g_hWnd, SW_SHOWDEFAULT);
        }
        ::UpdateWindow(g_hWnd);

        if (fullScreen) {
            toggleFullscreen(true);
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.IniFilename = NULL; // app manages its own config

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

        return 0;
    }

    void beginFrame() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void render(bool vsync) {
        // Handle minimized/occluded
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            return;
        }
        g_SwapChainOccluded = false;

        // Handle deferred resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Rendering
        ImGui::Render();
        const ImVec4& cc = gui::themeManager.clearColor;
        const float clear_color_with_alpha[4] = { cc.x * cc.w, cc.y * cc.w, cc.z * cc.w, cc.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(vsync ? 1u : 0u, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    void getMouseScreenPos(double& x, double& y) {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(g_hWnd, &p);
        x = (double)p.x;
        y = (double)p.y;
    }

    void setMouseScreenPos(double x, double y) {
        POINT origin{ 0, 0 };
        ClientToScreen(g_hWnd, &origin);
        SetCursorPos(origin.x + (int)x, origin.y + (int)y);
    }

    int renderLoop() {
        // Main loop
        MSG msg;
        while (true) {
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                    return 0;
            }

            // Window state and size tracking
            bool nowMax = IsZoomed(g_hWnd) != 0;
            if (nowMax != _maximized) {
                _maximized = nowMax;
                core::configManager.acquire();
                core::configManager.conf["maximized"] = _maximized;
                core::configManager.release(true);
            }

            RECT rc{};
            GetClientRect(g_hWnd, &rc);
            _winWidth = rc.right - rc.left;
            _winHeight = rc.bottom - rc.top;

            if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
                fullScreen = !fullScreen;
                toggleFullscreen(fullScreen);
                core::configManager.acquire();
                core::configManager.conf["fullscreen"] = fullScreen;
                core::configManager.release();
            }

            if ((_winWidth != winWidth || _winHeight != winHeight) && !_maximized && _winWidth > 0 && _winHeight > 0 && !fullScreen) {
                winWidth = _winWidth;
                winHeight = _winHeight;
                core::configManager.acquire();
                core::configManager.conf["windowSize"]["w"] = winWidth;
                core::configManager.conf["windowSize"]["h"] = winHeight;
                core::configManager.release(true);
            }

            beginFrame();

            if (_winWidth > 0 && _winHeight > 0) {
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2((float)_winWidth, (float)_winHeight));
                gui::mainWindow.draw();
            }

            render();
        }

        return 0;
    }

    int end() {
        // Cleanup
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        if (g_hWnd) {
            ::DestroyWindow(g_hWnd);
            g_hWnd = nullptr;
        }
        if (g_wc.lpszClassName)
            ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);

        return 0;
    }

    ImTextureID createTexture(int width, int height, const void* data) {
        ID3D11ShaderResourceView* out_srv;
        // Create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D11Texture2D* pTexture = NULL;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = data;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &out_srv);
        pTexture->Release();

        return (ImTextureID)(uintptr_t)out_srv;
    }

    void updateTexture(ImTextureID texId, const void* data) {
        if (!texId || !data)
            return;

        ID3D11ShaderResourceView* srv =
            (ID3D11ShaderResourceView*)(uintptr_t)texId;

        ID3D11Resource* resource = nullptr;
        srv->GetResource(&resource);
        if (!resource)
            return;

        // We know this is a Texture2D
        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                              (void**)&texture);
        resource->Release();

        if (FAILED(hr) || !texture)
            return;

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        // Update entire texture (mip 0)
        g_pd3dDeviceContext->UpdateSubresource(
            texture,
            0,       // Subresource (mip 0)
            nullptr, // Entire resource
            data,
            desc.Width * 4, // Row pitch (RGBA8)
            0               // Slice pitch (unused for 2D)
        );

        texture->Release();
    }

    void deleteTexture(ImTextureID texId) {
        ID3D11ShaderResourceView* out_srv = (ID3D11ShaderResourceView*)texId;

        out_srv->Release();
    }
}

// Helper functions (adapted from Dear ImGui example)
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2,
                                                D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}