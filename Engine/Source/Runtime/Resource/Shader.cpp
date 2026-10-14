#include "Shader.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Scene/GameProject.h"
#include <d3d12shader.h>
#include <filesystem>
#include <map>
#include <chrono>
#include <stdexcept>
#include "AssetArchive.h"
#include <atomic>
#pragma comment(lib, "dxcompiler.lib")
namespace AtomEngine
{
namespace
{
using Microsoft::WRL::ComPtr;
std::map<std::wstring, std::shared_ptr<Shader>> shaders;
std::map<std::filesystem::path, std::filesystem::file_time_type> files;
std::string error;
auto lastPoll = std::chrono::steady_clock::now();
void Check(HRESULT h)
{
    if (FAILED(h))
        throw std::runtime_error("DXC operation failed: " + std::to_string(h));
}
auto Scan()
{
    std::map<std::filesystem::path, std::filesystem::file_time_type> result;
    if (GameArchiveMounted()) return result;
    for (auto &f : std::filesystem::recursive_directory_iterator(ResolveAssetFile("Asset/Shaders")))
        if (f.is_regular_file() && (f.path().extension() == L".hlsl" || f.path().extension() == L".hlsli"))
            result.emplace(f.path(), f.last_write_time());
    return result;
}
class ArchiveIncludeHandler final : public IDxcIncludeHandler
{
    std::atomic<ULONG> references{1};
    ComPtr<IDxcUtils> utils;
public:
    explicit ArchiveIncludeHandler(IDxcUtils* value) : utils(value) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDxcIncludeHandler)) { *object = static_cast<IDxcIncludeHandler*>(this); AddRef(); return S_OK; }
        *object=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { auto count=--references; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob** output) override
    {
        if (!output || !filename) return E_POINTER; *output=nullptr;
        try {
            auto path = std::filesystem::path(filename);
            if (!AssetFileExists(path)) path = std::filesystem::path("Asset/Shaders") / path.relative_path();
            auto bytes=ReadAssetBytes(path); ComPtr<IDxcBlobEncoding> blob;
            auto result=utils->CreateBlob(bytes->data(),UINT32(bytes->size()),DXC_CP_UTF8,&blob);
            if (FAILED(result)) return result;
            *output=blob.Detach(); return S_OK;
        } catch (...) { return E_FAIL; }
    }
};
} // namespace
Shader::Shader(std::wstring f, std::wstring p, std::wstring e)
    : mFile(std::move(f)), mProfile(std::move(p)), mEntry(std::move(e))
{
    mSource = ResolveAssetFile(std::filesystem::path(L"Asset/Shaders") / std::filesystem::path(mFile).relative_path());
    mIncludeDirectory = ResolveAssetFile("Asset/Shaders");
}
void Shader::Compile()
{
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcIncludeHandler> include;
    Check(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
    Check(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
    if (GameArchiveMounted()) include.Attach(new ArchiveIncludeHandler(utils.Get()));
    else Check(utils->CreateDefaultIncludeHandler(&include));
    auto path = mSource;
    ComPtr<IDxcBlobEncoding> source;
    auto bytes = ReadAssetBytes(path);
    auto loadResult = utils->CreateBlob(bytes->data(), UINT32(bytes->size()), DXC_CP_UTF8, &source);
    if (FAILED(loadResult))
        throw std::runtime_error("Cannot load shader: " + path.string());
    DxcBuffer buffer{source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8};
    auto shaderDir = mIncludeDirectory;
    auto engineShaders = EngineDirectory() / L"Asset/Shaders";
    auto nativeDir = std::filesystem::absolute("Source/Runtime/Platform/DirectX12/Shader");
    const wchar_t *args[] = {path.c_str(), L"-E", mEntry.c_str(),    L"-T", mProfile.c_str(), L"-Zi", L"-Qembed_debug",
                             L"-Zpr",      L"-I", shaderDir.c_str(), L"-I", engineShaders.c_str(), L"-I", nativeDir.c_str()};
    ComPtr<IDxcResult> result;
    Check(compiler->Compile(&buffer, args, uint32_t(std::size(args)), include.Get(), IID_PPV_ARGS(&result)));
    HRESULT status;
    Check(result->GetStatus(&status));
    ComPtr<IDxcBlobUtf8> diagnostics;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&diagnostics), nullptr);
    if (FAILED(status))
        throw std::runtime_error(diagnostics ? diagnostics->GetStringPointer() : "Shader compile failed");
    ComPtr<IDxcBlob> bytecode, reflectionData;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr));
    Check(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr));
    DxcBuffer reflectionBuffer{reflectionData->GetBufferPointer(), reflectionData->GetBufferSize(), 0};
    ComPtr<ID3D12ShaderReflection> reflection;
    Check(utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection)));
    D3D12_SHADER_DESC desc;
    Check(reflection->GetDesc(&desc));
    std::vector<ShaderBinding> bindings;
    for (uint32_t i = 0; i < desc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC b;
        Check(reflection->GetResourceBindingDesc(i, &b));
        ShaderBinding binding{b.Name, uint32_t(b.Type), b.BindPoint, b.BindCount, b.Space};
        if (b.Type == D3D_SIT_CBUFFER)
        {
            auto cb = reflection->GetConstantBufferByName(b.Name);
            D3D12_SHADER_BUFFER_DESC d;
            Check(cb->GetDesc(&d));
            binding.size = d.Size;
            for (uint32_t v = 0; v < d.Variables; ++v)
            {
                D3D12_SHADER_VARIABLE_DESC vd;
                Check(cb->GetVariableByIndex(v)->GetDesc(&vd));
                binding.variables.push_back({vd.Name, vd.StartOffset, vd.Size});
            }
        }
        bindings.push_back(std::move(binding));
    }
    mBytecode = std::move(bytecode);
    mBindings = std::move(bindings);
    ++mRevision;
}
std::shared_ptr<Shader> ShaderLibrary::Get(const std::wstring &f, const std::wstring &p, const std::wstring &e)
{
    auto key = ResolveAssetFile(std::filesystem::path(L"Asset/Shaders") / std::filesystem::path(f).relative_path()).wstring() + L"|" + p + L"|" + e;
    auto it = shaders.find(key);
    if (it != shaders.end())
        return it->second;
    auto shader = std::make_shared<Shader>(f, p, e);
    shader->Compile();
    shaders.emplace(key, shader);
    if (files.empty())
        files = Scan();
    return shader;
}
bool ShaderLibrary::PollChanges()
{
    if (GameArchiveMounted()) return false;
    auto now = std::chrono::steady_clock::now();
    if (now - lastPoll < std::chrono::milliseconds(500))
        return false;
    lastPoll = now;
    try
    {
        auto updated = Scan();
        if (updated == files)
            return false;
        files = updated;
        std::vector<std::pair<std::shared_ptr<Shader>, Shader>> pending;
        for (auto &[key, s] : shaders)
        {
            Shader candidate = *s;
            candidate.Compile();
            pending.emplace_back(s, std::move(candidate));
        }
        for (auto &[s, c] : pending)
            *s = std::move(c);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        WriteLog(LogLevel::Error,"Shaders",error);
        return false;
    }
}
const std::string &ShaderLibrary::Error()
{
    return error;
}
std::vector<std::shared_ptr<Shader>> ShaderLibrary::All()
{
    std::vector<std::shared_ptr<Shader>> result;
    for (auto &[k, s] : shaders)
        result.push_back(s);
    return result;
}
std::shared_ptr<Shader> ShaderLibrary::FindBytecode(IDxcBlob *blob)
{
    for (auto &[key, s] : shaders)
        if (s->Bytecode() == blob)
            return s;
    return nullptr;
}
void ShaderLibrary::ReportError(const std::string &message)
{
    if(error!=message) WriteLog(LogLevel::Error,"Shaders",message);
    error = message;
}
void ShaderLibrary::Forget(const std::shared_ptr<Shader> &shader)
{
    for (auto it = shaders.begin(); it != shaders.end();)
        if (it->second == shader)
            it = shaders.erase(it);
        else
            ++it;
}
void ShaderLibrary::Clear()
{
    shaders.clear();
    files.clear();
    error.clear();
}
} // namespace AtomEngine
