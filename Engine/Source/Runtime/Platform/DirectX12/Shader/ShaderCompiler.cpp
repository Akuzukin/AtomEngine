#include "ShaderCompiler.h"
#include "Runtime/Resource/Shader.h"
namespace AtomEngine {
Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileBlob(const std::wstring&file,const wchar_t*profile,const wchar_t*entry){return ShaderLibrary::Get(file,profile,entry)->Bytecode();}
}
