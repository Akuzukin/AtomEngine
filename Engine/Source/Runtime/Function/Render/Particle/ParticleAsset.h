#pragma once
#include "ParticleSettingsCodec.h"
#include "Runtime/Resource/AssetArchive.h"
#include <filesystem>
#include <fstream>
#include <Windows.h>

namespace AtomEngine
{
inline ParticleSettings LoadParticleAsset(const std::filesystem::path &path)
{
    return ReadParticleSettings(ReadAssetText(path));
}

inline void SaveParticleAsset(const std::filesystem::path &path, const ParticleSettings &settings)
{
    if (path.extension() != L".particle") throw std::runtime_error("Particle assets must use .particle extension");
    auto validated = ReadParticleSettings(ParticleJson(settings).dump());
    auto text = ParticleJson(validated).dump(2) + "\n";
    auto absolute = std::filesystem::absolute(path);
    std::filesystem::create_directories(absolute.parent_path());
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempFileNameW(absolute.parent_path().c_str(), L"atp", 0, temporary))
        throw std::runtime_error("Cannot create particle asset temporary file");
    try
    {
        {std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), std::streamsize(text.size()));stream.flush();
        if (!stream) throw std::runtime_error("Cannot write particle asset");}
        if (!MoveFileExW(temporary, absolute.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace particle asset");
    }
    catch (...) { DeleteFileW(temporary); throw; }
}
}
