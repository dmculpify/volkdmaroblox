#pragma once
#include <thread>
#include <chrono>
#include <memory>
#include <logger/logger.h>
#include <render/render.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define TEST_NAME "notepad.exe"
#define BINARY_NAME "RobloxPlayerBeta.exe"
#define sleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#define safe_continue(x) std::this_thread::sleep_for(std::chrono::milliseconds(x)); continue
inline HWND roblox_window;
inline std::unique_ptr<logger_t> logger = std::make_unique<logger_t>();
inline std::unique_ptr<render_t> render = std::make_unique<render_t>();
inline std::uintptr_t base = 0;
inline std::uintptr_t size = 0;
namespace v2
{
	void exit(const char* message);
}