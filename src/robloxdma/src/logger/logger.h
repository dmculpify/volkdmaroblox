#pragma once
#include <chrono>
enum level_t
{
	info,
	debug,
	warning,
	error
};
class logger_t final
{
public:
	template <level_t level, typename... A>
	inline void log(const char* format, A&&... args)
	{
		const char* color;
		const char* label;
		if constexpr (level == info) { color = "\x1b[92m"; label = "INFO"; }
		else if constexpr (level == debug) { color = "\x1b[94m"; label = "DEBUG"; }
		else if constexpr (level == warning) { color = "\x1b[33m"; label = "WARN"; }
		else if constexpr (level == error) { color = "\x1b[31m"; label = "ERROR"; }
		auto now = std::chrono::system_clock::now();
		std::time_t t = std::chrono::system_clock::to_time_t(now);
		std::tm tm;
		localtime_s(&tm, &t);
		char timebuf[9];
		std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);
		std::printf("%s[%s][%s]\033[0m ", color, timebuf, label);
		std::printf(format, args...);
		std::printf("\n");
		std::fflush(stdout);
	}
};