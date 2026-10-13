# Added dependencies

- Jolt Physics 5.5.0: vendored source under Jolt, MIT license in Jolt/LICENSE. Built by CMake with 16-bit object layers and dynamic MSVC runtime.
- TinyXML2 11.0.0: tinyxml2/tinyxml2.h and .cpp, zlib license in tinyxml2/LICENSE.txt.
- .NET 10.0.12 native host headers: dotnet/hostfxr.h, nethost.h and coreclr_delegates.h from Microsoft.NETCore.App.Host.win-x64. License and third-party notices in dotnet. nethost.dll is copied from the installed .NET 10 x64 SDK; the .NET runtime is a runtime prerequisite, not vendored.

Upstream sources: https://github.com/jrouwe/JoltPhysics , https://github.com/leethomason/tinyxml2 , https://github.com/dotnet/runtime .
