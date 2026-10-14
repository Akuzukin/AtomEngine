#include "AssetArchive.h"
#include "Runtime/Core/Utility/Utility.h"
#include <Windows.h>
#include <bcrypt.h>
#include "json.hpp"
#include <array>
#include <fstream>
#include <map>
#include <mutex>
#include <algorithm>
#include <stdexcept>
#pragma comment(lib, "bcrypt.lib")
namespace AtomEngine
{
namespace
{
using Bytes = std::vector<uint8_t>;
void CryptoCheck(NTSTATUS status) { if (status < 0) throw std::runtime_error("Asset archive authentication/decryption failed"); }
struct Algorithm
{
    BCRYPT_ALG_HANDLE handle = nullptr;
    explicit Algorithm(LPCWSTR name) { CryptoCheck(BCryptOpenAlgorithmProvider(&handle, name, nullptr, 0)); }
    ~Algorithm() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
struct Key
{
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~Key() { if (handle) BCryptDestroyKey(handle); }
};
struct Hash
{
    BCRYPT_HASH_HANDLE handle = nullptr;
    ~Hash() { if (handle) BCryptDestroyHash(handle); }
};
void ReadExact(std::ifstream& stream, void* data, size_t size)
{
    if (size && !stream.read(static_cast<char*>(data), std::streamsize(size))) throw std::runtime_error("Truncated asset archive");
}
template<size_t N> std::array<uint8_t, N> Hex(const std::string& value)
{
    if (value.size() != N * 2) throw std::runtime_error("Invalid archive crypto metadata");
    std::array<uint8_t, N> result{};
    auto digit = [](char c) -> int { if (c >= '0' && c <= '9') return c-'0'; if (c >= 'A' && c <= 'F') return c-'A'+10; if (c >= 'a' && c <= 'f') return c-'a'+10; throw std::runtime_error("Invalid archive hex"); };
    for (size_t i=0; i<N; ++i) result[i] = uint8_t(digit(value[i*2])*16+digit(value[i*2+1]));
    return result;
}
Bytes Decrypt(const Bytes& cipher, const std::array<uint8_t,32>& secret, const std::array<uint8_t,12>& nonce,
              const std::array<uint8_t,16>& tag, const std::string& aad)
{
    Algorithm aes(BCRYPT_AES_ALGORITHM);
    CryptoCheck(BCryptSetProperty(aes.handle, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0));
    Key key; CryptoCheck(BCryptGenerateSymmetricKey(aes.handle, &key.handle, nullptr, 0, const_cast<PUCHAR>(secret.data()), ULONG(secret.size()), 0));
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info; BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce.data()); info.cbNonce = ULONG(nonce.size());
    info.pbTag = const_cast<PUCHAR>(tag.data()); info.cbTag = ULONG(tag.size());
    info.pbAuthData = (PUCHAR)aad.data(); info.cbAuthData = ULONG(aad.size());
    Bytes plain(std::max<size_t>(1, cipher.size())); ULONG written = 0;
    CryptoCheck(BCryptDecrypt(key.handle, const_cast<PUCHAR>(cipher.data()), ULONG(cipher.size()), &info, nullptr, 0, plain.data(), ULONG(plain.size()), &written, 0));
    plain.resize(written); return plain;
}
struct Entry { uint64_t offset, size; std::array<uint8_t,12> nonce; std::array<uint8_t,16> tag; std::string name; };
struct Archive
{
    std::filesystem::path file, root;
    std::array<uint8_t,32> secret{};
    uint64_t dataOffset = 0;
    std::map<std::string, Entry> entries;
    ~Archive() { SecureZeroMemory(secret.data(), secret.size()); }
    std::string Name(const std::filesystem::path& path) const
    {
        auto normalized = path.lexically_normal();
        auto relative = normalized.is_absolute() ? normalized.lexically_relative(root) : normalized;
        for (auto& part : relative) if (part == L"..") return {};
        if (relative.empty() || relative.has_root_path()) return {};
        auto name = WStringToUTF8(relative.lexically_normal().generic_wstring());
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return name;
    }
};
std::shared_ptr<Archive> mounted;
std::mutex mountMutex;
std::shared_ptr<Archive> Current() { std::lock_guard guard(mountMutex); return mounted; }
}
void MountGameArchive(const std::filesystem::path& executable, const std::filesystem::path& archiveFile)
{
    auto archive = std::make_shared<Archive>(); archive->file = std::filesystem::absolute(archiveFile).lexically_normal(); archive->root = archive->file.parent_path();
    std::ifstream exe(executable, std::ios::binary | std::ios::ate);
    if (!exe || exe.tellg() < std::streamoff(72)) throw std::runtime_error("Missing game build bootstrap");
    exe.seekg(-72, std::ios::end); std::array<char,8> magic{}; ReadExact(exe, magic.data(), magic.size());
    if (std::string(magic.data(),magic.size()) != "ATRUN001") throw std::runtime_error("Missing game build bootstrap");
    ReadExact(exe, archive->secret.data(), 32); std::array<uint8_t,32> expected{}; ReadExact(exe, expected.data(),32);
    std::ifstream input(archive->file, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Missing game.atpak");
    auto length = uint64_t(input.tellg()); input.seekg(0);
    // 整包摘要绑定到本次游戏构建，任何位置的篡改都在 GPU 初始化之前拒绝。
    Algorithm sha(BCRYPT_SHA256_ALGORITHM); Hash hash; CryptoCheck(BCryptCreateHash(sha.handle,&hash.handle,nullptr,0,nullptr,0,0));
    std::array<uint8_t,65536> buffer{};
    while (input) { input.read((char*)buffer.data(), buffer.size()); auto count=input.gcount(); if(count) CryptoCheck(BCryptHashData(hash.handle,buffer.data(),ULONG(count),0)); }
    std::array<uint8_t,32> actual{}; CryptoCheck(BCryptFinishHash(hash.handle,actual.data(),ULONG(actual.size()),0));
    if (actual != expected) throw std::runtime_error("Asset archive integrity check failed");
    input.clear(); input.seekg(0); ReadExact(input,magic.data(),8);
    if (std::string(magic.data(),8) != "ATPAK001") throw std::runtime_error("Unsupported asset archive");
    uint32_t indexSize=0; ReadExact(input,&indexSize,4);
    if (!indexSize || indexSize > 64*1024*1024 || uint64_t(indexSize)+40 > length) throw std::runtime_error("Invalid archive index size");
    std::array<uint8_t,12> nonce{}; std::array<uint8_t,16> tag{}; ReadExact(input,nonce.data(),12); ReadExact(input,tag.data(),16);
    Bytes cipher(indexSize); ReadExact(input,cipher.data(),cipher.size()); archive->dataOffset = uint64_t(input.tellg());
    auto plain = Decrypt(cipher,archive->secret,nonce,tag,"ATPAK001"); auto index = nlohmann::json::parse(plain);
    if (index.at("version") != 1 || !index.at("files").is_array() || index.at("files").size() > 1000000) throw std::runtime_error("Unsupported asset index");
    uint64_t end = 0;
    for (auto& item : index.at("files"))
    {
        Entry entry{ item.at("offset").get<uint64_t>(), item.at("size").get<uint64_t>(), Hex<12>(item.at("nonce")), Hex<16>(item.at("tag")), item.at("path").get<std::string>() };
        auto name = archive->Name(UTF8ToWString(entry.name));
        if (name.empty() || entry.name.size() > 32768 || entry.offset != end || entry.size > 512ull*1024*1024 || entry.offset > length-archive->dataOffset || entry.size > length-archive->dataOffset-entry.offset || !archive->entries.emplace(name,entry).second)
            throw std::runtime_error("Invalid asset archive entry");
        end += entry.size;
    }
    if (end != length-archive->dataOffset) throw std::runtime_error("Invalid archive payload length");
    std::lock_guard guard(mountMutex); mounted = std::move(archive);
}
void UnmountGameArchive() { std::lock_guard guard(mountMutex); mounted.reset(); }
bool GameArchiveMounted() { return bool(Current()); }
bool AssetFileExists(const std::filesystem::path& path)
{
    if (auto archive = Current()) return archive->entries.contains(archive->Name(path));
    return std::filesystem::is_regular_file(path);
}
std::shared_ptr<Bytes> ReadAssetBytes(const std::filesystem::path& path)
{
    if (auto archive = Current())
    {
        auto entry = archive->entries.find(archive->Name(path));
        if (entry == archive->entries.end()) throw std::runtime_error("Missing packaged asset: " + WStringToUTF8(path.wstring()));
        std::ifstream input(archive->file,std::ios::binary); input.seekg(std::streamoff(archive->dataOffset+entry->second.offset));
        Bytes cipher(size_t(entry->second.size)); ReadExact(input,cipher.data(),cipher.size());
        return std::make_shared<Bytes>(Decrypt(cipher,archive->secret,entry->second.nonce,entry->second.tag,entry->second.name));
    }
    std::ifstream input(path,std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot read asset: " + WStringToUTF8(path.wstring()));
    auto size = input.tellg(); if (size < 0 || size > std::streamoff(512ull*1024*1024)) throw std::runtime_error("Asset exceeds supported size");
    auto bytes=std::make_shared<Bytes>(size_t(size)); input.seekg(0); ReadExact(input,bytes->data(),bytes->size()); return bytes;
}
std::string ReadAssetText(const std::filesystem::path& path) { auto bytes=ReadAssetBytes(path); return std::string(bytes->begin(),bytes->end()); }
std::vector<std::string> GameArchiveEntries() { std::vector<std::string> names; if(auto archive=Current()) for(auto& [key,entry]:archive->entries) names.push_back(entry.name); return names; }
}
