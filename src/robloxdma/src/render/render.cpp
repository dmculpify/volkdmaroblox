#define IMGUI_DEFINE_MATH_OPERATORS
#include <render/render.h>
#include <render/assets/fonts.h>

#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <array>
#include <globals.h>
#include <settings/settings.h>
#include <settings/config.h>
#include <cheats/esp/esp.h>
#include <cheats/aimbot/aimbot.h>
#include <cache/cache.h>
#include <dma_helper.h>
#include <VolkDMA/process.hh>
#include <VolkDMA/inputstate.hh>
#include <sdk/offsets/offsets.h>

#define ALPHA    ( ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )
#define NO_ALPHA ( ImGuiColorEditFlags_NoTooltip    | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
    {
        return true;
    }

    switch (msg)
    {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

render_t::render_t()
{
    this->detail = std::make_unique<detail_t>();
}

render_t::~render_t()
{
    destroy_imgui();
    destroy_window();
    destroy_device();
}

bool render_t::create_window()
{
    this->detail->window_class.cbSize = sizeof(this->detail->window_class);
    this->detail->window_class.style = CS_CLASSDC;
    this->detail->window_class.lpszClassName = "despair";
    this->detail->window_class.hInstance = GetModuleHandleA(0);
    this->detail->window_class.lpfnWndProc = wnd_proc;

    RegisterClassExA(&this->detail->window_class);

    int window_width = (settings::performance::resolution_width > 0) ? settings::performance::resolution_width : GetSystemMetrics(SM_CXSCREEN);
    int window_height = (settings::performance::resolution_height > 0) ? settings::performance::resolution_height : GetSystemMetrics(SM_CYSCREEN);
    
    this->detail->window = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        this->detail->window_class.lpszClassName,
        "despair",
        WS_POPUP,
        0,
        0,
        window_width,
        window_height,
        0,
        0,
        this->detail->window_class.hInstance,
        0
    );

    if (!this->detail->window)
    {
        return false;
    }

    SetLayeredWindowAttributes(this->detail->window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

    RECT clientArea{};
    RECT windowArea{};

    GetClientRect(this->detail->window, &clientArea);
    GetWindowRect(this->detail->window, &windowArea);

    POINT diff{};
    ClientToScreen(this->detail->window, &diff);

    MARGINS margins
    {
        windowArea.left + (diff.x - windowArea.left),
        windowArea.top + (diff.y - windowArea.top),
        windowArea.right,
        windowArea.bottom,
    };

    DwmExtendFrameIntoClientArea(this->detail->window, &margins);

    ShowWindow(this->detail->window, SW_SHOW);
    UpdateWindow(this->detail->window);

    return true;
}

#ifndef DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 0x00000200
#endif
#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200
#endif

bool render_t::create_device()
{
    D3D_FEATURE_LEVEL feature_level;
    D3D_FEATURE_LEVEL feature_level_list[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT result = E_FAIL;

    auto try_create = [&](UINT buffer_count, DXGI_SWAP_EFFECT swap_effect, UINT flags, D3D_DRIVER_TYPE driver) -> HRESULT {
        if (this->detail->swap_chain) { this->detail->swap_chain->Release(); this->detail->swap_chain = nullptr; }
        if (this->detail->device_context) { this->detail->device_context->Release(); this->detail->device_context = nullptr; }
        if (this->detail->device) { this->detail->device->Release(); this->detail->device = nullptr; }
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = buffer_count;
        scd.BufferDesc.Width = 0;
        scd.BufferDesc.Height = 0;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 0;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.OutputWindow = this->detail->window;
        scd.SwapEffect = swap_effect;
        scd.Windowed = 1;
        scd.Flags = flags;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        return D3D11CreateDeviceAndSwapChain(
            nullptr, driver, nullptr, 0,
            feature_level_list, 2, D3D11_SDK_VERSION,
            &scd, &this->detail->swap_chain, &this->detail->device,
            &feature_level, &this->detail->device_context
        );
    };

    result = try_create(2, DXGI_SWAP_EFFECT_FLIP_DISCARD,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
        D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(result))
        result = try_create(2, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(result))
        result = try_create(2, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, D3D_DRIVER_TYPE_WARP);
    if (FAILED(result))
        result = try_create(1, DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(result))
        result = try_create(1, DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, D3D_DRIVER_TYPE_WARP);

    if (FAILED(result) || !this->detail->swap_chain || !this->detail->device || !this->detail->device_context)
    {
        char msg[256];
        sprintf_s(msg, sizeof(msg),
            "DirectX 11 graphics device could not be created (error 0x%08X).\n\n"
            "Update your graphics drivers or try running on a PC with a dedicated GPU.", (unsigned)result);
        MessageBoxA(nullptr, msg, "Graphics Error", MB_ICONERROR | MB_OK);
        return false;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    this->detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (!back_buffer)
    {
        MessageBoxA(nullptr, "Failed to get swap chain back buffer.", "Graphics Error", MB_ICONERROR | MB_OK);
        return false;
    }
    result = this->detail->device->CreateRenderTargetView(back_buffer, nullptr, &this->detail->render_target_view);
    back_buffer->Release();
    if (FAILED(result) || !this->detail->render_target_view)
    {
        MessageBoxA(nullptr, "Failed to create render target view.", "Graphics Error", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

bool render_t::create_imgui()
{
    using namespace ImGui;
    CreateContext();
    StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    io.Fonts->AddFontDefault();
    esp_font = io.Fonts->AddFontFromMemoryTTF(visitor_font, sizeof visitor_font, 9.333f);

    if (!ImGui_ImplWin32_Init(this->detail->window))
    {
        return false;
    }

    if (!this->detail->device || !this->detail->device_context)
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(this->detail->device, this->detail->device_context))
    {
        return false;
    }

    return true;
}

void render_t::destroy_device()
{
    if (this->detail->render_target_view) this->detail->render_target_view->Release();
    if (this->detail->swap_chain) this->detail->swap_chain->Release();
    if (this->detail->device_context) this->detail->device_context->Release();
    if (this->detail->device) this->detail->device->Release();
}

void render_t::destroy_window()
{
    DestroyWindow(this->detail->window);
    UnregisterClassA(this->detail->window_class.lpszClassName, this->detail->window_class.hInstance);
}

void render_t::destroy_imgui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool render_t::resize_window(int width, int height)
{
    if (!this->detail->window || !this->detail->swap_chain || !this->detail->device_context)
        return false;

    if (this->detail->render_target_view)
    {
        this->detail->render_target_view->Release();
        this->detail->render_target_view = nullptr;
    }

    SetWindowPos(this->detail->window, HWND_TOPMOST, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    RECT clientArea{};
    RECT windowArea{};
    GetClientRect(this->detail->window, &clientArea);
    GetWindowRect(this->detail->window, &windowArea);

    POINT diff{};
    ClientToScreen(this->detail->window, &diff);

    MARGINS margins
    {
        windowArea.left + (diff.x - windowArea.left),
        windowArea.top + (diff.y - windowArea.top),
        windowArea.right,
        windowArea.bottom,
    };
    DwmExtendFrameIntoClientArea(this->detail->window, &margins);

    HRESULT hr = this->detail->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr))
    {
        printf("[Resolution] ResizeBuffers failed with HRESULT: 0x%08X\n", hr);
        return false;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    hr = this->detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    
    if (SUCCEEDED(hr) && back_buffer)
    {
        hr = this->detail->device->CreateRenderTargetView(back_buffer, nullptr, &this->detail->render_target_view);
        back_buffer->Release();
        
        if (SUCCEEDED(hr))
        {
            D3D11_VIEWPORT viewport = {};
            viewport.TopLeftX = 0;
            viewport.TopLeftY = 0;
            viewport.Width = static_cast<float>(width);
            viewport.Height = static_cast<float>(height);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            this->detail->device_context->RSSetViewports(1, &viewport);
            
            UpdateWindow(this->detail->window);
            ShowWindow(this->detail->window, SW_SHOW);
            return true;
        }
    }

    return false;
}

void render_t::start_render()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (settings::performance::resolution_changed)
    {
        settings::performance::resolution_changed = false;
        printf("[Resolution] Attempting to change to %dx%d\n", settings::performance::resolution_width, settings::performance::resolution_height);
        
        if (resize_window(settings::performance::resolution_width, settings::performance::resolution_height))
        {
            printf("[Resolution] Successfully changed to %dx%d\n", settings::performance::resolution_width, settings::performance::resolution_height);
        }
        else
        {
            printf("[Resolution] Failed to change resolution\n");
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    ImGuiIO& io = ImGui::GetIO();
    RECT rect;
    if (GetClientRect(this->detail->window, &rect))
    {
        io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
    }

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        render->running = !render->running;

        if (render->running)
        {
            SetWindowLong(this->detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
        else
        {
            SetWindowLong(this->detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
    }
}

void render_t::end_render()
{
    ImGui::Render();

    RECT rect;
    if (GetClientRect(this->detail->window, &rect))
    {
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = static_cast<float>(rect.right - rect.left);
        viewport.Height = static_cast<float>(rect.bottom - rect.top);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        this->detail->device_context->RSSetViewports(1, &viewport);
    }

    float clear_color[4]{ 0, 0, 0, 0 };
    this->detail->device_context->OMSetRenderTargets(1, &this->detail->render_target_view, nullptr);
    this->detail->device_context->ClearRenderTargetView(this->detail->render_target_view, clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    this->detail->swap_chain->Present(0, 0);
}

void render_t::render_menu()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImVec2 menu_size = ImVec2(590, 450);
    float header_height = ImGui::GetFontSize() + style.WindowPadding.y * 2 - style.WindowBorderSize * 3;

    ImGui::SetNextWindowPos(io.DisplaySize / 2 - menu_size / 2, ImGuiCond_Once);
    ImGui::SetNextWindowSize(menu_size, ImGuiCond_Once);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    
    ImGui::Begin("Roblox DMA###main", &running);

    ImGui::BeginChild("Main", ImVec2(0, 0));
    {
        ImGui::TextUnformatted("Aimbot");
        ImGui::Separator();
        
        ImGui::Checkbox("Enable Aimbot", &settings::aimbot::enabled);
        ImGui::Checkbox("Aimbot Team Check", &settings::aimbot::team_check);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Skip teammates when aiming");
        ImGui::Checkbox("Draw FOV Circle", &settings::aimbot::draw_fov);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show FOV circle and crosshair");
        const char* body_parts[] = { "Head", "Chest (UpperTorso)", "Body (HumanoidRootPart)" };
        if (ImGui::Combo("Aim at", &settings::aimbot::aim_body_part, body_parts, IM_ARRAYSIZE(body_parts)))
            { }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bone/part to aim at (R15/R6 compatible)");
        static bool waiting_for_aim_key = false;
        static std::array<bool, 256> last_aim_key_state = {};
        const char* aim_key_label = "Set Aim Key";
        for (const auto& vk : InputState::virtual_keys) {
            if (vk.code == (settings::aimbot::aim_key & 0xFF)) {
                aim_key_label = vk.name.data();
                break;
            }
        }
        if (waiting_for_aim_key) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("Press any key (keyboard or mouse)..."))
                waiting_for_aim_key = false;
            ImGui::PopStyleColor();
            InputState* inp = get_input_state();
            if (inp && inp->read_bitmap()) {
                for (const auto& vk : InputState::virtual_keys) {
                    if (vk.code == 0xFF) continue;
                    bool down = inp->is_key_down(vk.code);
                    if (down && !last_aim_key_state[vk.code]) {
                        settings::aimbot::aim_key = vk.code;
                        waiting_for_aim_key = false;
                        break;
                    }
                    last_aim_key_state[vk.code] = down;
                }
            }
        } else {
            if (ImGui::Button(aim_key_label))
                waiting_for_aim_key = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Key to hold for aiming (reads from game PC via DMA)");
        
        // Target priority selection
        static const char* priority_names[] = { 
            "Closest to Crosshair", 
            "Closest Distance" 
        };
        
        if (ImGui::Combo("Target Priority", &settings::aimbot::target_priority, priority_names, IM_ARRAYSIZE(priority_names)))
        {
            // Value updated directly
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How to select target when multiple enemies in FOV");
        
        ImGui::SliderFloat("FOV", &settings::aimbot::fov, 10.0f, 180.0f, "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Field of view for target selection");
        
        ImGui::SliderFloat("Smoothness", &settings::aimbot::smoothness, 1.0f, 20.0f, "%.1f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("1.0 = instant, higher = smoother");
        
        ImGui::Checkbox("Prediction", &settings::aimbot::prediction);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Predict target movement based on velocity");
        
        if (settings::aimbot::prediction)
        {
            ImGui::SliderFloat("Prediction Strength", &settings::aimbot::prediction_strength, 0.1f, 3.0f, "%.1f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0.5 = conservative, 1.0 = balanced, 2.0 = aggressive");
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextUnformatted("Triggerbot");
        ImGui::Separator();
        
        ImGui::Checkbox("Enable Triggerbot", &settings::triggerbot::enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically shoots when hovering over enemy");
        
        ImGui::Checkbox("Triggerbot Team Check", &settings::triggerbot::team_check);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Don't shoot teammates");
        
        ImGui::Checkbox("Head Only", &settings::triggerbot::head_only);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Only trigger when hovering over head");
        static bool waiting_for_trigger_key = false;
        static std::array<bool, 256> last_trigger_key_state = {};
        const char* trigger_key_label = "Set Trigger Key";
        for (const auto& vk : InputState::virtual_keys) {
            if (vk.code == (settings::triggerbot::trigger_key & 0xFF)) {
                trigger_key_label = vk.name.data();
                break;
            }
        }
        if (waiting_for_trigger_key) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("Press any key (trigger)..."))
                waiting_for_trigger_key = false;
            ImGui::PopStyleColor();
            InputState* inp = get_input_state();
            if (inp && inp->read_bitmap()) {
                for (const auto& vk : InputState::virtual_keys) {
                    if (vk.code == 0xFF) continue;
                    bool down = inp->is_key_down(vk.code);
                    if (down && !last_trigger_key_state[vk.code]) {
                        settings::triggerbot::trigger_key = vk.code;
                        waiting_for_trigger_key = false;
                        break;
                    }
                    last_trigger_key_state[vk.code] = down;
                }
            }
        } else {
            if (ImGui::Button(trigger_key_label))
                waiting_for_trigger_key = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Key to hold for triggering (reads from game PC via DMA)");
        
        ImGui::SliderFloat("Delay (ms)", &settings::triggerbot::delay_ms, 0.0f, 500.0f, "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delay before shooting (humanization)");
        
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextUnformatted("Visuals");
        ImGui::Separator();
        
        ImGui::Checkbox("Enable Visuals", &settings::visuals::master);
        
        ImGui::Checkbox("Team Check (ESP)", &settings::visuals::team_check);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Don't render ESP for teammates");

        ImGui::SliderFloat("Max Render Distance (m)", &settings::visuals::max_render_distance, 0.0f, 2000.0f, "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Maximum distance to render ESP (0 = unlimited)");

        ImGui::Checkbox("Box", &settings::visuals::box);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Box Color", (float*)&settings::visuals::colors::box_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Filled Box", &settings::visuals::filled_box);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Box Fill Color", (float*)&settings::visuals::colors::box_fill_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Chams", &settings::visuals::chams);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Chams Color", (float*)&settings::visuals::colors::chams_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Chams Outline", &settings::visuals::chams_outline);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Chams Outline Color", (float*)&settings::visuals::colors::chams_outline_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Skeleton", &settings::visuals::skeleton);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Skeleton Color", (float*)&settings::visuals::colors::skeleton_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Names", &settings::visuals::name);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Names Color", (float*)&settings::visuals::colors::names_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Distance", &settings::visuals::distance);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Distance Color", (float*)&settings::visuals::colors::distance_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Status", &settings::visuals::status);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Status Color", (float*)&settings::visuals::colors::status_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Tool", &settings::visuals::tool);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Tool Color", (float*)&settings::visuals::colors::tool_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Healthbar", &settings::visuals::healthbar);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Healthbar Color", (float*)&settings::visuals::colors::healthbar_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Armourbar", &settings::visuals::armor);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x);
        ImGui::ColorEdit4("##Armourbar Color", (float*)&settings::visuals::colors::armourbar_color, ImGuiColorEditFlags_NoInputs);

        ImGui::Separator();
        ImGui::TextUnformatted("Performance");
        ImGui::Checkbox("Low-end mode", &settings::performance::low_end_mode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Disables anti-aliasing for better FPS on weak PCs");
        
        ImGui::Separator();
        ImGui::TextUnformatted("Resolution");
        
        static const char* resolutions[] = { "1280x720", "1920x1080", "2560x1440", "3840x2160", "Custom" };
        static int current_resolution = 1; // Default to 1920x1080
        
        if (ImGui::Combo("Preset", &current_resolution, resolutions, IM_ARRAYSIZE(resolutions)))
        {
            switch (current_resolution)
            {
                case 0: settings::performance::resolution_width = 1280; settings::performance::resolution_height = 720; break;
                case 1: settings::performance::resolution_width = 1920; settings::performance::resolution_height = 1080; break;
                case 2: settings::performance::resolution_width = 2560; settings::performance::resolution_height = 1440; break;
                case 3: settings::performance::resolution_width = 3840; settings::performance::resolution_height = 2160; break;
            }
        }
        
        if (current_resolution == 4) // Custom
        {
            ImGui::InputInt("Width", &settings::performance::resolution_width);
            ImGui::InputInt("Height", &settings::performance::resolution_height);
        }
        else
        {
            ImGui::Text("Width: %d", settings::performance::resolution_width);
            ImGui::Text("Height: %d", settings::performance::resolution_height);
        }
        
        if (ImGui::Button("Apply Resolution", ImVec2(-1, 0)))
        {
            if (settings::performance::resolution_width >= 640 && settings::performance::resolution_width <= 7680 &&
                settings::performance::resolution_height >= 480 && settings::performance::resolution_height <= 4320)
            {
                printf("[Resolution] Button clicked - setting flag to change to %dx%d\n", 
                    settings::performance::resolution_width, settings::performance::resolution_height);
                settings::performance::resolution_changed = true;
            }
            else
            {
                printf("[Resolution] Invalid resolution: %dx%d\n", 
                    settings::performance::resolution_width, settings::performance::resolution_height);
                ImGui::OpenPopup("Invalid Resolution");
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Apply the selected resolution (640x480 to 7680x4320)");
        
        if (ImGui::BeginPopupModal("Invalid Resolution", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Resolution must be between 640x480 and 7680x4320");
            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        ImGui::Separator();
        ImGui::TextUnformatted("FPS Settings");
        
        ImGui::Text("Current FPS: %.1f", this->fps);
        
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextUnformatted("Whitelist");
        ImGui::Separator();
        
        ImGui::TextWrapped("Select players to exclude from ESP and aimbot:");
        ImGui::Spacing();
        
        if (ImGui::Button("Force Refresh Players", ImVec2(-1, 0)))
        {
            settings::performance::force_refresh = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Manually refresh the player cache");
        
        ImGui::Spacing();
        std::vector<cache::entity_t> players_snapshot;
        {
            std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
            players_snapshot = cache::frame_data.entities;
        }
        
        // Display whitelist with checkboxes
        if (players_snapshot.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No players found. Join a game or click Force Refresh.");
        }
        else
        {
            std::vector<std::uint64_t> name_ptrs;
            std::vector<std::string> usernames;
            Process* proc = get_process();
            if (proc)
            {
                for (const auto& p : players_snapshot)
                    if (p.userid != 0)
                        name_ptrs.push_back(0);
                if (!name_ptrs.empty())
                {
                    void* name_scatter = proc->create_scatter(0);
                    if (name_scatter)
                    {
                        size_t j = 0;
                        for (const auto& p : players_snapshot)
                        {
                            if (p.userid == 0) continue;
                            proc->add_read_scatter(name_scatter, p.instance.address + Offsets::Instance::Name, &name_ptrs[j], sizeof(std::uint64_t));
                            ++j;
                        }
                        proc->execute_scatter(name_scatter);
                        proc->close_scatter(name_scatter);
                    }
                    read_strings_scatter(proc, name_ptrs, usernames);
                }
            }
            ImGui::BeginChild("WhitelistScroll", ImVec2(0, 150), true);
            size_t name_idx = 0;
            for (const auto& player : players_snapshot)
            {
                if (player.userid == 0)
                    continue;
                bool is_whitelisted = config::is_whitelisted(player.userid);
                std::string username = (name_idx < usernames.size()) ? usernames[name_idx++] : "Unknown";
                char label[256];
                if (player.name != username && !player.name.empty())
                {
                    snprintf(label, sizeof(label), "@%s (%s) - %dm###player_%llu", 
                        username.c_str(), player.name.c_str(), player.distance, player.userid);
                }
                else
                {
                    snprintf(label, sizeof(label), "@%s - %dm###player_%llu", 
                        username.c_str(), player.distance, player.userid);
                }
                
                if (ImGui::Checkbox(label, &is_whitelisted))
                {
                    if (is_whitelisted)
                    {
                        config::add_to_whitelist(player.userid);
                    }
                    else
                    {
                        config::remove_from_whitelist(player.userid);
                    }
                }
                
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Username: @%s", username.c_str());
                    if (!player.name.empty() && player.name != username)
                        ImGui::Text("Display Name: %s", player.name.c_str());
                    ImGui::Text("UserID: %llu", player.userid);
                    ImGui::Text("Health: %.0f/%.0f", player.health, player.max_health);
                    ImGui::Text("Team: %llu", player.team);
                    ImGui::EndTooltip();
                }
            }
            
            ImGui::EndChild();
            
            ImGui::Text("Whitelisted: %zu / %zu players", config::whitelisted_players.size(), players_snapshot.size());
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Clear All Whitelist", ImVec2(-1, 0)))
        {
            config::whitelisted_players.clear();
            config::save();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove all players from whitelist");
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();

    ImGui::End();
    static auto last_save_time = std::chrono::high_resolution_clock::now();
    auto save_current_time = std::chrono::high_resolution_clock::now();
    auto save_elapsed = std::chrono::duration_cast<std::chrono::seconds>(save_current_time - last_save_time).count();
    if (save_elapsed >= 5)
    {
        config::save();
        last_save_time = save_current_time;
    }
}


void render_t::render_visuals()
{
    ImGuiIO& io = ImGui::GetIO();
    
    static int frame_count = 0;
    static auto last_time = std::chrono::high_resolution_clock::now();
    
    frame_count++;
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count();
    
    if (elapsed >= 500)
    {
        this->fps = (frame_count * 1000.0f) / elapsed;
        frame_count = 0;
        last_time = current_time;
    }
    
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 120, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(110, 40), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    if (ImGui::Begin("##FPS_Counter", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        ImVec4 fps_color;
        if (this->fps >= 100.0f) 
            fps_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
        else if (this->fps >= 60.0f) 
            fps_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        else 
            fps_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
        
        ImGui::TextColored(fps_color, "FPS: %.0f", this->fps);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    
    esp::run();
    aimbot::draw_fov_circle();
}