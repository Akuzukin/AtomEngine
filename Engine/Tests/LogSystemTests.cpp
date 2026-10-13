#include "Runtime/Core/LogSystem/LogSystem.h"
#include <thread>
#include <fstream>
#include <stdexcept>
#include <iostream>
namespace AtomEngine {
std::string WStringToUTF8(const std::wstring& text) {
    int length=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(length,' '); WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),length,nullptr,nullptr); return result;
}}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() {
    using namespace AtomEngine;
    try {
        LogOptions options; options.directory="generated/LogTests";
        options.maxFileBytes=1024; options.maxFiles=3; options.historySize=8;
        StartLog(options);
        auto path=GetLogFile();
        std::string longText="中文 "+std::string(4096,'x'); Log("%s",longText.c_str());
        Check(GetLogHistory().back().message==longText,"Long UTF-8 log was truncated");
        SetLogLevel(LogLevel::Error); auto count=GetLogHistory().size();
        WriteLog(LogLevel::Info,"Filter","suppressed"); Check(GetLogHistory().size()==count,"Severity filter failed");
        SetLogLevel(LogLevel::Info);
        std::vector<std::thread> threads;
        for(int t=0;t<4;++t)threads.emplace_back([t] {for(int i=0;i<100;++i)WriteLog(LogLevel::Info,"Thread",std::to_string(t)+":"+std::to_string(i));});
        for(auto& t:threads)t.join();
        auto entries=GetLogHistory();Check(entries.size()==8,"History limit failed");
        for(size_t i=1;i<entries.size();++i)Check(entries[i].sequence>entries[i-1].sequence,"Concurrent sequence order failed");
        Log(L"%ls",L"宽字符日志");Check(GetLogHistory().back().message=="宽字符日志","Wide UTF-8 log failed");
        FlushLog();CloseLog();
        Check(std::filesystem::exists(path.string()+".1") && std::filesystem::exists(path.string()+".2") &&
              !std::filesystem::exists(path.string()+".3"),"File rotation limit failed");
        std::ifstream stream(path,std::ios::binary); std::string text((std::istreambuf_iterator<char>(stream)),{});
        Check(text.find("宽字符日志")!=std::string::npos,"File sink did not flush UTF-8");
        StartLog(options); WriteLog(LogLevel::Error,"Restart","reopened");CloseLog();
        std::cout<<"Log tests passed: UTF-8, long messages, severity, concurrent history, rotation, flush and restart\n";
        return 0;
    }catch(const std::exception& e){CloseLog();std::cerr<<e.what()<<'\n';return 1;}
}
