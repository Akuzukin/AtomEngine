#include "LogSystem.h"
#include <fstream>
#include <algorithm>
#include <cstdarg>
#include <deque>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <thread>
namespace AtomEngine
{
	namespace
	{
		struct Logger
		{
			std::mutex mutex;
			std::ofstream stream;
			LogOptions options;
			std::filesystem::path file;
			std::deque<LogEntry> history;
			uint64_t sequence = 0;
			size_t bytes = 0;
		};
		Logger& State() { static Logger state; return state; }
		std::string Timestamp()
		{
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			std::tm local{}; localtime_s(&local, &time);
			std::ostringstream out;
			out << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
				<< std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
			return out.str();
		}
		void Rotate(Logger& s)
		{
			s.stream.close();
			std::error_code ec;
			for (size_t i = s.options.maxFiles; i > 1; --i)
			{
				auto to = s.file; to += "." + std::to_string(i - 1);
				auto from = s.file; if (i > 2) from += "." + std::to_string(i - 2);
				std::filesystem::remove(to, ec); ec.clear();
				if (std::filesystem::exists(from, ec)) std::filesystem::rename(from, to, ec);
			}
			s.stream.open(s.file, std::ios::binary | std::ios::trunc); s.bytes = 0;
		}
		std::string Format(const char* format, va_list args)
		{
			if (!format) return {};
			va_list count; va_copy(count, args); int size = _vscprintf(format, count); va_end(count);
			if (size < 0) return "Invalid log format";
			std::vector<char> buffer(size_t(size) + 1);
			vsnprintf(buffer.data(), buffer.size(), format, args);
			return { buffer.data(), size_t(size) };
		}
	}
	const char* LogLevelName(LogLevel l)
	{
		static const char* names[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL" };
		return names[std::clamp(int(l), 0, 5)];
	}
	void StartLog(const LogOptions& options)
	{
		auto& s = State(); std::lock_guard lock(s.mutex);
		if (s.stream.is_open()) s.stream.close();
		s.options = options; s.options.maxFiles = std::clamp(options.maxFiles, size_t(1), size_t(32));
		s.options.maxFileBytes = std::max(options.maxFileBytes, size_t(1024));
		s.options.historySize = std::max(options.historySize, size_t(1));
		s.history.clear();
		if (s.options.directory.empty())
		{
			wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
			s.options.directory = std::filesystem::path(exe).parent_path() / "logs";
		}
		std::error_code ec; std::filesystem::create_directories(s.options.directory, ec);
		s.file = s.options.directory / "AtomEngine.log";
		s.bytes = 0; s.stream.open(s.file, std::ios::binary | std::ios::app);
		if (s.stream) s.bytes = size_t(std::filesystem::file_size(s.file, ec));
		else OutputDebugStringA("AtomEngine: cannot open log file; debugger/history sinks remain available.\n");
	}
	void CloseLog() { auto& s = State(); std::lock_guard lock(s.mutex); s.stream.close(); }
	void FlushLog() { auto& s = State(); std::lock_guard lock(s.mutex); s.stream.flush(); }
	void SetLogLevel(LogLevel l) { auto& s = State(); std::lock_guard lock(s.mutex); s.options.level = l; }
	void ClearLogHistory() { auto& s = State(); std::lock_guard lock(s.mutex); s.history.clear(); }
	std::vector<LogEntry> GetLogHistory() { auto& s = State(); std::lock_guard lock(s.mutex); return { s.history.begin(),s.history.end() }; }
	std::filesystem::path GetLogFile() { auto& s = State(); std::lock_guard lock(s.mutex); return s.file; }
	void WriteLog(LogLevel l, std::string_view category, std::string_view message, std::source_location location)
	{
		auto& s = State(); std::lock_guard lock(s.mutex);
		if (l < s.options.level) return;
		std::string text(message); while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
		std::ostringstream out; out << Timestamp() << " [" << LogLevelName(l) << "] [" << category << "] [t:"
			<< std::this_thread::get_id() << "] " << text;
		if (l >= LogLevel::Error) out << " (" << location.file_name() << ':' << location.line() << ')';
		auto formatted = out.str();
		s.history.push_back({ ++s.sequence,l,std::string(category),std::move(text),formatted });
		while (s.history.size() > s.options.historySize) s.history.pop_front();
		if (s.stream.is_open())
		{
			if (s.bytes && s.bytes + formatted.size() + 1 > s.options.maxFileBytes) Rotate(s);
			s.stream << formatted << '\n'; s.bytes += formatted.size() + 1;
			if (l >= LogLevel::Warning) s.stream.flush();
		}
		formatted += '\n'; OutputDebugStringA(formatted.c_str());
	}
	void Log(const char* format, ...) { va_list args; va_start(args, format); auto text = Format(format, args); va_end(args); WriteLog(LogLevel::Info, "Engine", text); }
	void LogDebug(const char* format, ...) { va_list args; va_start(args, format); auto text = Format(format, args); va_end(args); WriteLog(LogLevel::Debug, "ECS", text); }
	void Log(const wchar_t* format, ...)
	{
		va_list args; va_start(args, format); va_list count; va_copy(count, args);
		int size = format ? _vscwprintf(format, count) : -1; va_end(count);
		std::vector<wchar_t> buffer(size >= 0 ? size_t(size) + 1 : 1);
		if (size >= 0) vswprintf_s(buffer.data(), buffer.size(), format, args);
		va_end(args); WriteLog(LogLevel::Info, "Engine", size >= 0 ? WStringToUTF8(buffer.data()) : "Invalid wide log format");
	}
	ScopedLogTimer::ScopedLogTimer(std::string c, std::string n) :mCategory(std::move(c)), mName(std::move(n)), mStart(std::chrono::steady_clock::now())
	{
		WriteLog(LogLevel::Info, mCategory, mName + " started");
	}
	ScopedLogTimer::~ScopedLogTimer()
	{
		try
		{
			auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - mStart).count();
			WriteLog(LogLevel::Info, mCategory, mName + " finished in " + std::to_string(ms) + " ms");
		}
		catch (...) {}
	}
}
