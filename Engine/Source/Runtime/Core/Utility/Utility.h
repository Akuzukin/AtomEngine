#pragma once
#include "Runtime/Core/Math/Math.h"
#include <exception>
#include <iostream>
#include <vector>
#define NOMINMAX
#include <Windows.h>

namespace AtomEngine
{
	class com_exception : public std::runtime_error
	{
		inline std::string what(HRESULT hr)
		{
			char s_str[256] = {};
			sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
			return std::string(s_str);
		}
	public:
		com_exception(HRESULT hr) : std::runtime_error(what(hr)), result(hr) {}
		HRESULT Error() const { return result; }
	private:
		const HRESULT result;
	};

	// Helper utility converts D3D API failures into exceptions.
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw com_exception(hr);
		}
	}

	inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
	{
		if (FAILED(hr))
		{
			OutputDebugString(msg);
			throw com_exception(hr);
		}
	}

	inline void Print(const char* msg) { OutputDebugStringA(msg); }
	inline void Print(const wchar_t* msg) { OutputDebugStringW(msg); }

	inline void Printf(const char* format, ...)
	{
#ifdef _DEBUG

		char buffer[1024];
		va_list ap;
		va_start(ap, format);
		vsprintf_s(buffer, 1024, format, ap);
		va_end(ap);
		Print(buffer);
#endif
	}

	inline void Printf(const wchar_t* format, ...)
	{
#ifdef _DEBUG

		wchar_t buffer[1024];
		va_list ap;
		va_start(ap, format);
		vswprintf(buffer, 1024, format, ap);
		va_end(ap);
		Print(buffer);
#endif
	}
#ifdef _DEBUG
	inline void PrintSubMessage(const char* format, ...)
	{
		Print("--> ");
		char buffer[256];
		va_list ap;
		va_start(ap, format);
		vsprintf_s(buffer, 256, format, ap);
		va_end(ap);
		Print(buffer);
		Print("\n");
	}
	inline void PrintSubMessage(const wchar_t* format, ...)
	{
		Print("--> ");
		wchar_t buffer[256];
		va_list ap;
		va_start(ap, format);
		vswprintf(buffer, 256, format, ap);
		va_end(ap);
		Print(buffer);
		Print("\n");
	}
	inline void PrintSubMessage(void)
	{
	}
#endif

	std::wstring UTF8ToWString(const std::string& str);
	std::string WStringToUTF8(const std::wstring& str);
	std::string ToLower(const std::string& str);
	std::wstring ToLower(const std::wstring& str);
	std::string RemoveBasePath(const std::string& str);
	std::wstring RemoveBasePath(const std::wstring& str);
	std::string GetFileExtension(const std::string& str);
	std::wstring GetFileExtension(const std::wstring& str);
	std::string GetBasePath(const std::string& filePath);
	std::wstring GetBasePath(const std::wstring& filePath);
	std::string RemoveExtension(const std::string& str);
	std::wstring RemoveExtension(const std::wstring& str);
	std::wstring GetDirectoryFromPath(const std::wstring& fullPath);
	std::shared_ptr<std::vector<byte>> ReadFileSync(const std::wstring& fileName);

#ifdef _DEBUG

#define STRINGIFY(x) #x
#define STRINGIFY_BUILTIN(x) STRINGIFY(x)

#define ASSERT( isFalse, ... ) \
        if (!(bool)(isFalse)) { \
            Print("\nAssertion failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            PrintSubMessage("\'" #isFalse "\' is false"); \
            PrintSubMessage(__VA_ARGS__); \
            Print("\n"); \
            __debugbreak(); \
        }

#else

#define ASSERT( isTrue, ... ) (void)(isTrue)

#endif

	void SIMDMemCopy(void* __restrict Dest, const void* __restrict Source, size_t NumQuadwords);
	void SIMDMemFill(void* __restrict Dest, __m128 FillVector, size_t NumQuadwords);
}
