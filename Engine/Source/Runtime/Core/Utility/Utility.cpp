#include "Utility.h"
#include <string>
#include <locale>
#include <fstream>
#include <filesystem> 

namespace AtomEngine
{
	void SIMDMemCopy(void* __restrict _Dest, const void* __restrict _Source, size_t NumQuadwords)
	{
		ASSERT(IsAligned(_Dest, 16));
		ASSERT(IsAligned(_Source, 16));

		__m128i* __restrict Dest = (__m128i * __restrict)_Dest;
		const __m128i* __restrict Source = (const __m128i * __restrict)_Source;

		// Discover how many quadwords precede a cache line boundary.  Copy them separately.
		size_t InitialQuadwordCount = (4 - ((size_t)Source >> 4) & 3) & 3;
		if (InitialQuadwordCount > NumQuadwords)
			InitialQuadwordCount = NumQuadwords;

		switch (InitialQuadwordCount)
		{
		case 3: _mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));	 // Fall through
		case 2: _mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));	 // Fall through
		case 1: _mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));	 // Fall through
		default:
			break;
		}

		if (NumQuadwords == InitialQuadwordCount)
			return;

		Dest += InitialQuadwordCount;
		Source += InitialQuadwordCount;
		NumQuadwords -= InitialQuadwordCount;

		size_t CacheLines = NumQuadwords >> 2;

		switch (CacheLines)
		{
		default:
		case 10: _mm_prefetch((char*)(Source + 36), _MM_HINT_NTA);	// Fall through
		case 9:  _mm_prefetch((char*)(Source + 32), _MM_HINT_NTA);	// Fall through
		case 8:  _mm_prefetch((char*)(Source + 28), _MM_HINT_NTA);	// Fall through
		case 7:  _mm_prefetch((char*)(Source + 24), _MM_HINT_NTA);	// Fall through
		case 6:  _mm_prefetch((char*)(Source + 20), _MM_HINT_NTA);	// Fall through
		case 5:  _mm_prefetch((char*)(Source + 16), _MM_HINT_NTA);	// Fall through
		case 4:  _mm_prefetch((char*)(Source + 12), _MM_HINT_NTA);	// Fall through
		case 3:  _mm_prefetch((char*)(Source + 8), _MM_HINT_NTA);	// Fall through
		case 2:  _mm_prefetch((char*)(Source + 4), _MM_HINT_NTA);	// Fall through
		case 1:  _mm_prefetch((char*)(Source + 0), _MM_HINT_NTA);	// Fall through

			// Do four quadwords per loop to minimize stalls.
			for (size_t i = CacheLines; i > 0; --i)
			{
				// If this is a large copy, start prefetching future cache lines.  This also prefetches the
				// trailing quadwords that are not part of a whole cache line.
				if (i >= 10)
					_mm_prefetch((char*)(Source + 40), _MM_HINT_NTA);

				_mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));
				_mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));
				_mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));
				_mm_stream_si128(Dest + 3, _mm_load_si128(Source + 3));

				Dest += 4;
				Source += 4;
			}

		case 0:	// No whole cache lines to read
			break;
		}

		// Copy the remaining quadwords
		switch (NumQuadwords & 3)
		{
		case 3: _mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));	 // Fall through
		case 2: _mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));	 // Fall through
		case 1: _mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));	 // Fall through
		default:
			break;
		}

		_mm_sfence();
	}

	void SIMDMemFill(void* __restrict _Dest, __m128 FillVector, size_t NumQuadwords)
	{
		ASSERT(IsAligned(_Dest, 16));

		const __m128i Source = _mm_castps_si128(FillVector);
		__m128i* __restrict Dest = (__m128i * __restrict)_Dest;

		switch (((size_t)Dest >> 4) & 3)
		{
		case 1: _mm_stream_si128(Dest++, Source); --NumQuadwords;	 // Fall through
		case 2: _mm_stream_si128(Dest++, Source); --NumQuadwords;	 // Fall through
		case 3: _mm_stream_si128(Dest++, Source); --NumQuadwords;	 // Fall through
		default:
			break;
		}

		size_t WholeCacheLines = NumQuadwords >> 2;

		// Do four quadwords per loop to minimize stalls.
		while (WholeCacheLines--)
		{
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
		}

		// Copy the remaining quadwords
		switch (NumQuadwords & 3)
		{
		case 3: _mm_stream_si128(Dest++, Source);	 // Fall through
		case 2: _mm_stream_si128(Dest++, Source);	 // Fall through
		case 1: _mm_stream_si128(Dest++, Source);	 // Fall through
		default:
			break;
		}

		_mm_sfence();
	}

	std::wstring AtomEngine::UTF8ToWString(const std::string& str)
	{
		if (str.empty())
		{
			return std::wstring();
		}
		int strByteSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wStr(strByteSize, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wStr[0], strByteSize);
		wStr.erase(wStr.end() - 1);
		return wStr;
	}

	std::string AtomEngine::WStringToUTF8(const std::wstring& str)
	{
		if (str.empty())
		{
			return std::string();
		}
		int wStrByteSize = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string narrowStr(wStrByteSize, '\0');
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &narrowStr[0], wStrByteSize, nullptr, nullptr);
		narrowStr.erase(narrowStr.end() - 1);
		return narrowStr;
	}

	std::string ToLower(const std::string& str)
	{
		std::string lower_case = str;
		std::locale loc;
		for (char& s : lower_case)
			s = std::tolower(s, loc);
		return lower_case;
	}

	std::wstring ToLower(const std::wstring& str)
	{
		std::wstring lower_case = str;
		std::locale loc;
		for (wchar_t& s : lower_case)
			s = std::tolower(s, loc);
		return lower_case;
	}

	std::string RemoveBasePath(const std::string& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind('/')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else
			return filePath;
	}

	std::wstring RemoveBasePath(const std::wstring& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind(L'/')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else if ((lastSlash = filePath.rfind(L'\\')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else
			return filePath;
	}

	std::string GetFileExtension(const std::string& filePath)
	{
		std::string fileName = RemoveBasePath(filePath);
		size_t extOffset = fileName.rfind('.');
		if (extOffset == std::wstring::npos)
			return "";

		return fileName.substr(extOffset + 1);
	}

	std::wstring GetFileExtension(const std::wstring& filePath)
	{
		std::wstring fileName = RemoveBasePath(filePath);
		size_t extOffset = fileName.rfind(L'.');
		if (extOffset == std::wstring::npos)
			return L"";

		return fileName.substr(extOffset + 1);
	}

	std::string GetBasePath(const std::string& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind('/')) != std::string::npos)
			return filePath.substr(0, lastSlash + 1);
		else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
			return filePath.substr(0, lastSlash + 1);
		else
			return "";
	}

	std::wstring GetBasePath(const std::wstring& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind(L'/')) != std::wstring::npos)
			return filePath.substr(0, lastSlash + 1);
		else if ((lastSlash = filePath.rfind(L'\\')) != std::wstring::npos)
			return filePath.substr(0, lastSlash + 1);
		else
			return L"";
	}

	std::string RemoveExtension(const std::string& filePath)
	{
		return filePath.substr(0, filePath.rfind("."));
	}

	std::wstring RemoveExtension(const std::wstring& filePath)
	{
		return filePath.substr(0, filePath.rfind(L"."));
	}

	std::wstring GetDirectoryFromPath(const std::wstring& fullPath)
	{
		size_t pos = fullPath.find_last_of(L"/\\");
		return (pos != std::wstring::npos) ? fullPath.substr(0, pos + 1) : L"";
	}

	std::shared_ptr<std::vector<byte>> NullFile = std::make_shared<std::vector<byte> >(std::vector<byte>());

	std::shared_ptr<std::vector<byte>> ReadFileHelper(const std::wstring& fileName)
	{
		if (!std::filesystem::exists(fileName))
			return NullFile;

		std::ifstream file(fileName, std::ios::in | std::ios::binary);
		if (!file)
			return NullFile;

		auto fileSize = std::filesystem::file_size(fileName);

		std::shared_ptr<std::vector<byte>> byteArray = std::make_shared<std::vector<byte>>(fileSize);
		file.read((char*)byteArray->data(), byteArray->size());
		file.close();

		return byteArray;
	}

	std::shared_ptr<std::vector<byte>> ReadFileSync(const std::wstring& fileName)
	{
		return ReadFileHelper(*(std::make_shared<std::wstring>(fileName)));
	}
}
