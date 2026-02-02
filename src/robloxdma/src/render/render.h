#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx11.h>
enum ImFonts_ : int
{
	ImFont_Main,
	ImFont_Code
};
enum ImPages_ : int
{
	ImPage_Aimbot,
	ImPage_Visuals,
	ImPage_World,
	ImPage_Explorer,
	ImPage_Settings,
	ImPages_COUNT
};
inline ImFont* esp_font = nullptr;
struct detail_t {
	HWND window = nullptr;
	WNDCLASSEX window_class = {};
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* device_context = nullptr;
	ID3D11RenderTargetView* render_target_view = nullptr;
	IDXGISwapChain* swap_chain = nullptr;
};
class render_t {
public:
	render_t();
	~render_t();
	bool running = false;
	float fps = 0.0f;
	void start_render();
	void render_menu();
	void render_visuals();
	void end_render();
	bool create_device();
	bool create_window();
	bool create_imgui();
	void destroy_device();
	void destroy_window();
	void destroy_imgui();
	bool resize_window(int width, int height);
	std::unique_ptr<detail_t> detail = std::make_unique<detail_t>();
private:
private:
	std::int32_t m_iCurrentPage = 0;
	std::vector<const char*> m_Tabs =
	{
		"Ragebot",
		"Visuals",
		"World",
		"Explorer",
		"Settings"
	};
};