#pragma once
#include <filesystem>
#include <Windows.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
namespace AtomEngine
{
struct ShaderVariable
{
    std::string name;
    uint32_t offset = 0, size = 0;
};
struct ShaderBinding
{
    std::string name;
    uint32_t type = 0, bindPoint = 0, count = 0, space = 0, size = 0;
    std::vector<ShaderVariable> variables;
};
struct ShaderPreviewRequest
{
    std::string mesh = "Sphere";
    uint32_t width = 256, height = 256;
};
class Shader
{
  public:
    Shader(std::wstring file, std::wstring profile, std::wstring entry = L"main");
    void Compile();
    IDxcBlob *Bytecode() const
    {
        return mBytecode.Get();
    }
    const std::vector<ShaderBinding> &Bindings() const
    {
        return mBindings;
    }
    const std::wstring &File() const
    {
        return mFile;
    }
    uint64_t Revision() const
    {
        return mRevision;
    }
    // The preview renderer may register an offscreen render request handler.
    std::function<void(const Shader &, const ShaderPreviewRequest &)> preview;
    void RequestPreview(const ShaderPreviewRequest &r) const
    {
        if (preview)
            preview(*this, r);
    }

  private:
    std::wstring mFile, mProfile, mEntry;
    std::filesystem::path mSource, mIncludeDirectory;
    Microsoft::WRL::ComPtr<IDxcBlob> mBytecode;
    std::vector<ShaderBinding> mBindings;
    uint64_t mRevision = 0;
};
class ShaderLibrary
{
  public:
    static std::shared_ptr<Shader> Get(const std::wstring &file, const std::wstring &profile,
                                       const std::wstring &entry);
    static bool PollChanges();
    static const std::string &Error();
    static std::vector<std::shared_ptr<Shader>> All();
    static void Clear();
    static void Forget(const std::shared_ptr<Shader> &shader);
    static std::shared_ptr<Shader> FindBytecode(IDxcBlob *blob);
    static void ReportError(const std::string &message);
};
} // namespace AtomEngine
