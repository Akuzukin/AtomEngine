#pragma once
#include <filesystem>
#include <fstream>
#include <future>
#include <regex>
#include <Windows.h>
#include "Runtime/Core/Utility/Utility.h"
#include "Runtime/Function/Scene/GameProject.h"
namespace AtomEngine
{
inline std::filesystem::path EngineSourceDirectory()
{
    for (auto start : {EngineDirectory(), std::filesystem::current_path()})
        for (auto p = start; !p.empty(); p = p.parent_path())
        {
            if (std::filesystem::exists(p / "AtomGame.vcxproj")) return p;
            if (std::filesystem::exists(p / "Engine/AtomGame.vcxproj")) return p / "Engine";
            if (p == p.parent_path()) break;
        }
    throw std::runtime_error("Engine source/build tools not found");
}
class ScriptTools
{
  public:
    static std::filesystem::path ProjectDirectory()
    {
        if (ActiveProject.project && !ActiveProject.project->scriptProject.empty())
            return (ActiveProject.project->Root() / UTF8ToWString(ActiveProject.project->scriptProject)).parent_path();
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
        for (auto start : {std::filesystem::current_path(), std::filesystem::path(exe).parent_path()})
            for (auto p = start; !p.empty(); p = p.parent_path())
            {
                if (std::filesystem::exists(p / "Managed/GameScripts/GameScripts.csproj"))
                    return p / "Managed/GameScripts";
                if (std::filesystem::exists(p / "Engine/Managed/GameScripts/GameScripts.csproj"))
                    return p / "Engine/Managed/GameScripts";
                if (p == p.parent_path())
                    break;
            }
        throw std::runtime_error("GameScripts project not found");
    }
    static std::filesystem::path WorkspacePath()
    {
        return ProjectDirectory().parent_path() / L"AtomScripts.code-workspace";
    }
    static std::filesystem::path VsCodeExecutable()
    {
        auto installed = [](const std::filesystem::path &path) {
            std::error_code ec;
            return std::filesystem::is_regular_file(path, ec);
        };
        wchar_t buffer[32768]{};
        for (auto name : {L"Code.exe", L"code.cmd"})
        {
            auto length = SearchPathW(nullptr, name, nullptr, DWORD(std::size(buffer)), buffer, nullptr);
            if (length && length < std::size(buffer))
            {
                auto path = std::filesystem::path(buffer);
                if (std::wstring(name) == L"code.cmd")
                    path = path.parent_path().parent_path() / L"Code.exe";
                if (installed(path))
                    return path;
            }
        }
        for (auto variable : {L"LOCALAPPDATA", L"ProgramFiles", L"ProgramFiles(x86)"})
        {
            auto length = GetEnvironmentVariableW(variable, buffer, DWORD(std::size(buffer)));
            if (!length || length >= std::size(buffer))
                continue;
            auto path = std::filesystem::path(buffer);
            if (std::wstring(variable) == L"LOCALAPPDATA")
                path /= L"Programs";
            path /= L"Microsoft VS Code/Code.exe";
            if (installed(path))
                return path;
        }
        throw std::runtime_error("VS Code not found; install VS Code or add its bin directory to PATH");
    }
    static std::filesystem::path Generate(const std::string &name, bool overrides,
                                          const std::filesystem::path &directory = {})
    {
        if (!std::regex_match(name, std::regex("[A-Za-z_][A-Za-z0-9_]*")))
            throw std::runtime_error("Enter a valid C# class name");
        auto path = (directory.empty() ? ProjectDirectory() : directory) / (name + ".cs");
        if (std::filesystem::exists(path))
            throw std::runtime_error("Script already exists");
        std::string source = "using Atom;\n\n// Open AtomScripts.code-workspace in VS Code for engine API completion and documentation.\nnamespace Game;\n\npublic sealed class @" + name + " : Behaviour\n{\n";
        if (overrides)
            source += "    public override void OnCreate() { }\n"
                      "    public override void OnUpdate(float dt) { }\n"
                      "    public override void OnFixedUpdate(float dt) { }\n"
                      "    public override void OnCollision(uint other, bool trigger) { }\n"
                      "    public override void OnUiClick() { }\n"
                      "    public override void OnDestroy() { }\n";
        source += "}\n";
        // CREATE_NEW keeps an existing file intact even if another editor creates it concurrently.
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create script file");
        DWORD written = 0;
        bool ok = WriteFile(file, source.data(), DWORD(source.size()), &written, nullptr);
        CloseHandle(file);
        if (!ok || written != source.size())
            throw std::runtime_error("Cannot write script file");
        return path;
    }
    static std::future<int> Build()
    {
        auto project = ActiveProject.project && !ActiveProject.project->scriptProject.empty()
            ? ActiveProject.project->Root() / UTF8ToWString(ActiveProject.project->scriptProject)
            : ProjectDirectory() / "GameScripts.csproj";
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
        auto output = std::filesystem::path(exe).parent_path() / "Scripts";
        auto log = output / "ScriptBuild.log";
        return std::async(std::launch::async, [project, output, log] {
            std::filesystem::create_directories(output);
            HANDLE file = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return -1;
            SetHandleInformation(file, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = si.hStdError = file;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION process{};
#ifdef RELEASE
            std::wstring config = L"Release";
#else
            std::wstring config = L"Debug";
#endif
            auto buildOutput = output / "Build";
            auto command = L"dotnet build \"" + project.wstring() + L"\" -c " + config + L" -o \"" +
                           buildOutput.wstring() + L"\" --nologo --ignore-failed-sources -p:AtomEngineSdkProject=\"" + (EngineSourceDirectory() / L"Managed/Atom.Managed/Atom.Managed.csproj").wstring() + L"\"";
            bool ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                     project.parent_path().c_str(), &si, &process);
            if (!ok)
            {
                CloseHandle(file);
                return -1;
            }
            WaitForSingleObject(process.hProcess, INFINITE);
            DWORD code = 1;
            GetExitCodeProcess(process.hProcess, &code);
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
            CloseHandle(file);
            if (!code)
                for (auto extension : {L".pdb", L".xml", L".deps.json", L".dll"})
                {
                    auto name = std::wstring(L"AtomScripts") + extension;
                    auto target = output / name, temp = output / (name + L".tmp");
                    std::error_code ec;
                    if (!std::filesystem::exists(buildOutput / name))
                        continue;
                    std::filesystem::copy_file(buildOutput / name, temp,
                                               std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec ||
                        !MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                        return -1;
                }
            return int(code);
        });
    }
};
} // namespace AtomEngine
