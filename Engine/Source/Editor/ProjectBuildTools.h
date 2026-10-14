#pragma once
#include "ScriptTools.h"
namespace AtomEngine
{
inline std::future<int> BuildProject(const std::filesystem::path& project, const std::filesystem::path& output, bool publish)
{
    auto engine = EngineSourceDirectory();
    return std::async(std::launch::async, [project, output, publish, engine] {
        auto logs = EngineDirectory() / L"logs"; std::filesystem::create_directories(logs);
        auto log = logs / L"ProjectBuild.log";
        HANDLE file = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return -1;
        SetHandleInformation(file, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        auto command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + (engine / L"Tools/Publish-Project.ps1").wstring()
            + L"\" -Project \"" + project.wstring() + L"\" -Output \"" + output.wstring() + L"\"" + (publish ? L"" : L" -BuildOnly");
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = startup.hStdError = file; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, engine.c_str(), &startup, &process)) { CloseHandle(file); return -1; }
        WaitForSingleObject(process.hProcess, INFINITE); DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(file); return int(code);
    });
}
}
