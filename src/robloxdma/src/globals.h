#pragma once
#include <thread>
#include <chrono>
#include <memory>
#include <logger/logger.h>
#include <render/render.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef DEBUG_INFO
#define LOG(fmt, ...) std::printf(fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) std::wprintf(fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...) ((void)0)
#define LOGW(fmt, ...) ((void)0)
#endif
#define BINARY_NAME "RobloxPlayerBeta.exe"
#define sleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
inline HWND roblox_window;
inline std::unique_ptr<logger_t> logger = std::make_unique<logger_t>();
inline std::unique_ptr<render_t> render = std::make_unique<render_t>();
inline std::uintptr_t base = 0;
inline std::uintptr_t size = 0;

class DMA;
class Process;
extern std::unique_ptr<DMA> g_dma;
extern std::unique_ptr<Process> g_process;

namespace v2
{
	void exit(const char* message);
}