#pragma once
#include "../Utility/Utility.h"
#include <chrono>
#include <filesystem>
#include <source_location>
#include <string_view>
namespace AtomEngine
{
	enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };
	struct LogEntry { uint64_t sequence; LogLevel level; std::string category, message, formatted; };
	struct LogOptions
	{
		std::filesystem::path directory; // Empty: executable directory / logs.
		LogLevel level = LogLevel::Info;
		size_t maxFileBytes = 8 * 1024 * 1024, maxFiles = 5, historySize = 2000;
	};
	void StartLog(const LogOptions& options = {});
	void CloseLog();
	void FlushLog();
	void SetLogLevel(LogLevel level);
	void ClearLogHistory();
	std::vector<LogEntry> GetLogHistory();
	std::filesystem::path GetLogFile();
	const char* LogLevelName(LogLevel level);
	void WriteLog(LogLevel level, std::string_view category, std::string_view message,
		std::source_location location = std::source_location::current());
	void Log(const char* format, ...);
	void Log(const wchar_t* format, ...);
	void LogDebug(const char* format, ...);
	class ScopedLogTimer
	{
	public:
		ScopedLogTimer(std::string category, std::string name);
		~ScopedLogTimer();
	private:
		std::string mCategory, mName;
		std::chrono::steady_clock::time_point mStart;
	};
}
