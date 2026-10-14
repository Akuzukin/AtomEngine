#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
namespace AtomEngine
{
// 发行资产只在内存解密；编辑器未挂载资产包时继续读取普通资源文件。
void MountGameArchive(const std::filesystem::path& executable, const std::filesystem::path& archive);
void UnmountGameArchive();
bool GameArchiveMounted();
bool AssetFileExists(const std::filesystem::path& path);
std::shared_ptr<std::vector<uint8_t>> ReadAssetBytes(const std::filesystem::path& path);
std::string ReadAssetText(const std::filesystem::path& path);
std::vector<std::string> GameArchiveEntries();
}
