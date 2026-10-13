param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$repository = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$fixtureRoot = Join-Path $repository 'generated/features-scripts'
$bridgeProject = Join-Path $repository 'Engine/Managed/Atom.Managed/Atom.Managed.csproj'
$captureRoot = Join-Path $repository "generated/outputs/x64/$Configuration/Smoke"
New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null
foreach ($version in 1,2) {
    $projectDir = Join-Path $fixtureRoot "v$version"
    New-Item -ItemType Directory -Path $projectDir -Force | Out-Null
    @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><AssemblyName>ReloadProbe</AssemblyName><ImplicitUsings>enable</ImplicitUsings></PropertyGroup>
  <ItemGroup><ProjectReference Include="$bridgeProject" /></ItemGroup>
</Project>
"@ | Set-Content -LiteralPath (Join-Path $projectDir 'ReloadProbe.csproj') -Encoding utf8
    @"
using Atom;
using System.Numerics;
namespace Game;
public sealed class ReloadProbe : Behaviour
{
    public float Speed = 2;
    public override void OnUpdate(float dt) { Object.Position += new Vector3(Speed * dt * $version, 0, 0); }
}
"@ | Set-Content -LiteralPath (Join-Path $projectDir 'ReloadProbe.cs') -Encoding utf8
    if ($version -eq 2) {
        @'
public sealed class AddedProbe : Behaviour
{
    public override void OnUpdate(float dt) { Object.Position += new Vector3(0, dt, 0); }
}
'@ | Add-Content -LiteralPath (Join-Path $projectDir 'ReloadProbe.cs') -Encoding utf8
    }
    dotnet build (Join-Path $projectDir 'ReloadProbe.csproj') -c $Configuration --nologo --ignore-failed-sources
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    Copy-Item -LiteralPath (Join-Path $projectDir "bin/$Configuration/net10.0/ReloadProbe.dll") -Destination (Join-Path $captureRoot "ReloadV$version.dll")
}
$generatedSource = Join-Path $captureRoot 'GeneratedBehaviour.cs'
if (Test-Path -LiteralPath $generatedSource) {
    $templateDir = Join-Path $fixtureRoot 'template'
    New-Item -ItemType Directory -Path $templateDir -Force | Out-Null
    @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><ImplicitUsings>enable</ImplicitUsings></PropertyGroup>
  <ItemGroup><ProjectReference Include="$bridgeProject" /><Compile Include="$generatedSource" /></ItemGroup>
</Project>
"@ | Set-Content -LiteralPath (Join-Path $templateDir 'GeneratedTemplate.csproj') -Encoding utf8
    dotnet build (Join-Path $templateDir 'GeneratedTemplate.csproj') -c $Configuration --nologo --ignore-failed-sources
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
