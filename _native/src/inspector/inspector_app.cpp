#include "hocloth/inspector/inspector_app.hpp"

#include "hocloth/inspector/inspector_ui.hpp"

#include <d3d11.h>
#include <dwmapi.h>
#include <iterator>
#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND handle, UINT message, WPARAM w_param, LPARAM l_param);

namespace hocloth::inspector {
namespace {

class InspectorApp;

constexpr wchar_t kInspectorWindowTitle[] = L"HoCloth Inspector";
InspectorApp* g_active_app = nullptr;
constexpr LONG kResizeBorderThickness = 8;
constexpr LONG kTitleBarHitHeight = 24;
constexpr LONG kTitleBarLeftControlReserve = 40;
constexpr LONG kTitleBarRightControlReserve = 44;
constexpr LONG kCollapsedWindowHeight = kTitleBarHitHeight + 1;

void EnableTransparentWindowComposition(HWND window_handle)
{
    const MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(window_handle, &margins);

    const LONG_PTR ex_style = GetWindowLongPtrW(window_handle, GWL_EXSTYLE);
    SetWindowLongPtrW(window_handle, GWL_EXSTYLE, ex_style | WS_EX_TOOLWINDOW);
}

void LoadInspectorFonts(float main_scale)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const char* font_path = "C:\\Windows\\Fonts\\msyh.ttc";
    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    font_config.PixelSnapH = false;

    const float font_size = 9.6f * main_scale;
    if (io.Fonts->AddFontFromFileTTF(font_path, font_size, &font_config, io.Fonts->GetGlyphRangesChineseFull()) == nullptr) {
        io.Fonts->AddFontDefault();
    }
}

void ApplyInspectorStyle(float main_scale, bool viewports_enabled)
{
    ImGuiStyle base_style;
    ImGui::StyleColorsDark(&base_style);
    base_style.ScaleAllSizes(main_scale);
    base_style.FontScaleDpi = main_scale;

    ImGuiStyle& style = ImGui::GetStyle();
    style = base_style;

    style.WindowRounding = viewports_enabled ? 0.0f : 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.15f, 0.17f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.14f, 0.16f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.23f, 0.25f, 0.28f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.22f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.26f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.30f, 0.35f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.32f, 0.29f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.40f, 0.35f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.47f, 0.37f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.25f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.29f, 0.33f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.35f, 0.40f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.18f, 0.20f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.30f, 0.34f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.25f, 0.29f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.54f, 0.80f, 0.61f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.54f, 0.80f, 0.61f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.63f, 0.87f, 0.69f, 1.0f);
    if (viewports_enabled) {
        colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}

class InspectorApp {
public:
    int Run()
    {
        g_active_app = this;
        ImGui_ImplWin32_EnableDpiAwareness();
        main_scale_ = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

        if (!CreateAppWindow()) {
            return 1;
        }

        if (!CreateDeviceD3D(window_handle_)) {
            DestroyAppWindow();
            return 1;
        }

        ShowWindow(window_handle_, SW_SHOWDEFAULT);
        UpdateWindow(window_handle_);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = false;
        ApplyInspectorStyle(main_scale_, false);
        LoadInspectorFonts(main_scale_);

        ImGui_ImplWin32_Init(window_handle_);
        ImGui_ImplDX11_Init(device_, device_context_);

        bool running = true;
        while (running) {
            MSG message = {};
            while (PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessage(&message);
                if (message.message == WM_QUIT) {
                    running = false;
                }
            }

            if (!running) {
                break;
            }

            if (swap_chain_occluded_ && swap_chain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
                Sleep(10);
                continue;
            }
            swap_chain_occluded_ = false;

            if (resize_width_ != 0 && resize_height_ != 0) {
                CleanupRenderTarget();
                swap_chain_->ResizeBuffers(0, resize_width_, resize_height_, DXGI_FORMAT_UNKNOWN, 0);
                resize_width_ = 0;
                resize_height_ = 0;
                CreateRenderTarget();
            }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            const bool host_visible = DrawInspectorUi();
            UpdateHostWindowState(host_visible);

            ImGui::Render();
            const float clear_color[4] = {0.0f, 0.0f, 0.0f, host_visible ? 0.0f : 0.0f};
            device_context_->OMSetRenderTargets(1, &render_target_view_, nullptr);
            device_context_->ClearRenderTargetView(render_target_view_, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            const HRESULT present_result = swap_chain_->Present(1, 0);
            swap_chain_occluded_ = (present_result == DXGI_STATUS_OCCLUDED);
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        DestroyAppWindow();
        g_active_app = nullptr;
        return 0;
    }

    void RequestClose()
    {
        if (window_handle_ != nullptr) {
            PostMessageW(window_handle_, WM_CLOSE, 0, 0);
        }
    }

    void UpdateHostWindowState(bool host_visible)
    {
        if (window_handle_ == nullptr) {
            return;
        }

        RECT current_rect = {};
        GetWindowRect(window_handle_, &current_rect);

        if (host_visible) {
            if (host_collapsed_) {
                const int restore_left = current_rect.left;
                const int restore_top = current_rect.top;
                const int restore_width = expanded_window_rect_.right - expanded_window_rect_.left;
                const int restore_height = expanded_window_rect_.bottom - expanded_window_rect_.top;
                SetWindowPos(
                    window_handle_,
                    nullptr,
                    restore_left,
                    restore_top,
                    restore_width,
                    restore_height,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                expanded_window_rect_.left = restore_left;
                expanded_window_rect_.top = restore_top;
                expanded_window_rect_.right = restore_left + restore_width;
                expanded_window_rect_.bottom = restore_top + restore_height;
                host_collapsed_ = false;
                return;
            }

            const int current_height = current_rect.bottom - current_rect.top;
            if (current_height > static_cast<int>(kCollapsedWindowHeight * main_scale_) + 4) {
                expanded_window_rect_ = current_rect;
            }
            return;
        }

        if (host_collapsed_) {
            const int expanded_height = expanded_window_rect_.bottom - expanded_window_rect_.top;
            const int current_width = current_rect.right - current_rect.left;
            expanded_window_rect_.left = current_rect.left;
            expanded_window_rect_.top = current_rect.top;
            expanded_window_rect_.right = current_rect.left + current_width;
            expanded_window_rect_.bottom = current_rect.top + expanded_height;
            return;
        }

        expanded_window_rect_ = current_rect;
        const int current_width = current_rect.right - current_rect.left;
        const int collapsed_height = static_cast<int>(kCollapsedWindowHeight * main_scale_);
        SetWindowPos(
            window_handle_,
            nullptr,
            current_rect.left,
            current_rect.top,
            current_width,
            collapsed_height,
            SWP_NOZORDER | SWP_NOACTIVATE);
        host_collapsed_ = true;
    }

    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param)
    {
        if (ImGui_ImplWin32_WndProcHandler(window_handle_, message, w_param, l_param)) {
            return true;
        }

        switch (message) {
        case WM_NCCALCSIZE:
            if (w_param == TRUE) {
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            const LRESULT hit = DefWindowProcW(window_handle_, message, w_param, l_param);
            if (hit != HTCLIENT) {
                return hit;
            }

            RECT window_rect = {};
            GetWindowRect(window_handle_, &window_rect);
            const LONG mouse_x = static_cast<LONG>(static_cast<short>(LOWORD(l_param)));
            const LONG mouse_y = static_cast<LONG>(static_cast<short>(HIWORD(l_param)));

            const bool on_left = mouse_x >= window_rect.left && mouse_x < window_rect.left + kResizeBorderThickness;
            const bool on_right = mouse_x <= window_rect.right && mouse_x > window_rect.right - kResizeBorderThickness;
            const bool on_top = mouse_y >= window_rect.top && mouse_y < window_rect.top + kResizeBorderThickness;
            const bool on_bottom = mouse_y <= window_rect.bottom && mouse_y > window_rect.bottom - kResizeBorderThickness;

            if (on_top && on_left) {
                return HTTOPLEFT;
            }
            if (on_top && on_right) {
                return HTTOPRIGHT;
            }
            if (on_bottom && on_left) {
                return HTBOTTOMLEFT;
            }
            if (on_bottom && on_right) {
                return HTBOTTOMRIGHT;
            }
            if (on_left) {
                return HTLEFT;
            }
            if (on_right) {
                return HTRIGHT;
            }
            if (on_top) {
                return HTTOP;
            }
            if (on_bottom) {
                return HTBOTTOM;
            }

            const LONG client_x = mouse_x - window_rect.left;
            const LONG client_y = mouse_y - window_rect.top;
            const LONG client_width = window_rect.right - window_rect.left;
            const bool within_drag_band =
                client_y >= 0 &&
                client_y < kTitleBarHitHeight &&
                client_x >= kTitleBarLeftControlReserve &&
                client_x < client_width - kTitleBarRightControlReserve;
            if (within_drag_band) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_SIZE:
            if (w_param == SIZE_MINIMIZED) {
                return 0;
            }
            resize_width_ = static_cast<UINT>(LOWORD(l_param));
            resize_height_ = static_cast<UINT>(HIWORD(l_param));
            return 0;
        case WM_CLOSE:
            DestroyWindow(window_handle_);
            return 0;
        case WM_SYSCOMMAND:
            if ((w_param & 0xfff0U) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(window_handle_, message, w_param, l_param);
    }

    static InspectorApp* GetFromWindow(HWND handle)
    {
        return reinterpret_cast<InspectorApp*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    }

private:
    bool CreateAppWindow()
    {
        window_class_ = {};
        window_class_.cbSize = sizeof(WNDCLASSEXW);
        window_class_.style = CS_CLASSDC;
        window_class_.lpfnWndProc = &InspectorApp::WindowProc;
        window_class_.hInstance = GetModuleHandleW(nullptr);
        window_class_.lpszClassName = L"HoClothInspectorWindow";
        RegisterClassExW(&window_class_);

        window_handle_ = CreateWindowW(
            window_class_.lpszClassName,
            kInspectorWindowTitle,
            WS_POPUP | WS_THICKFRAME,
            120,
            120,
            static_cast<int>(980.0f * main_scale_),
            static_cast<int>(720.0f * main_scale_),
            nullptr,
            nullptr,
            window_class_.hInstance,
            this);
        if (window_handle_ != nullptr) {
            EnableTransparentWindowComposition(window_handle_);
            SetWindowPos(window_handle_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        return window_handle_ != nullptr;
    }

    void DestroyAppWindow()
    {
        if (window_handle_ != nullptr) {
            DestroyWindow(window_handle_);
            window_handle_ = nullptr;
        }
        if (window_class_.lpszClassName != nullptr) {
            UnregisterClassW(window_class_.lpszClassName, window_class_.hInstance);
            window_class_ = {};
        }
    }

    bool CreateDeviceD3D(HWND handle)
    {
        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.BufferDesc.Width = 0;
        swap_chain_desc.BufferDesc.Height = 0;
        swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
        swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.OutputWindow = handle;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.Windowed = TRUE;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT create_device_flags = 0;
#ifdef _DEBUG
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        constexpr D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
        const HRESULT result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            create_device_flags,
            feature_levels,
            static_cast<UINT>(std::size(feature_levels)),
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &swap_chain_,
            &device_,
            &feature_level,
            &device_context_);

        if (result == DXGI_ERROR_UNSUPPORTED) {
            const HRESULT warp_result = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                create_device_flags,
                feature_levels,
                static_cast<UINT>(std::size(feature_levels)),
                D3D11_SDK_VERSION,
                &swap_chain_desc,
                &swap_chain_,
                &device_,
                &feature_level,
                &device_context_);
            if (FAILED(warp_result)) {
                return false;
            }
        } else if (FAILED(result)) {
            return false;
        }

        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D()
    {
        CleanupRenderTarget();
        if (swap_chain_ != nullptr) {
            swap_chain_->Release();
            swap_chain_ = nullptr;
        }
        if (device_context_ != nullptr) {
            device_context_->Release();
            device_context_ = nullptr;
        }
        if (device_ != nullptr) {
            device_->Release();
            device_ = nullptr;
        }
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* back_buffer = nullptr;
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        if (back_buffer != nullptr) {
            device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_view_);
            back_buffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (render_target_view_ != nullptr) {
            render_target_view_->Release();
            render_target_view_ = nullptr;
        }
    }

    static LRESULT WINAPI WindowProc(HWND handle, UINT message, WPARAM w_param, LPARAM l_param)
    {
        if (message == WM_NCCREATE) {
            auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
            auto* app = static_cast<InspectorApp*>(create_struct->lpCreateParams);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            if (app != nullptr) {
                app->window_handle_ = handle;
            }
        }

        if (auto* app = GetFromWindow(handle)) {
            return app->HandleMessage(message, w_param, l_param);
        }

        return DefWindowProcW(handle, message, w_param, l_param);
    }

    WNDCLASSEXW window_class_ = {};
    HWND window_handle_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* device_context_ = nullptr;
    IDXGISwapChain* swap_chain_ = nullptr;
    ID3D11RenderTargetView* render_target_view_ = nullptr;
    UINT resize_width_ = 0;
    UINT resize_height_ = 0;
    bool swap_chain_occluded_ = false;
    float main_scale_ = 1.0f;
    bool host_collapsed_ = false;
    RECT expanded_window_rect_ = {};
};

}  // namespace

int RunInspectorApp()
{
    InspectorApp app;
    return app.Run();
}

void RequestInspectorWindowClose()
{
    if (g_active_app != nullptr) {
        g_active_app->RequestClose();
    }
}

}  // namespace hocloth::inspector
