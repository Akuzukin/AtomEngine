param(
    [Parameter(Mandatory=$true)][string]$Project,
    [Parameter(Mandatory=$true)][string]$Output,
    [switch]$BuildOnly,
    [switch]$SkipNativeBuild
)
$ErrorActionPreference = 'Stop'
$engineSource = Split-Path -Parent $PSScriptRoot
$projectFile = (Resolve-Path -LiteralPath $Project).Path
$publishOutput = [IO.Path]::GetFullPath($Output)
$publisherArgs = @('--engine', $engineSource, '--project', $projectFile, '--output', $publishOutput)
if ($BuildOnly) { $publisherArgs += '--build-only' }
if ($SkipNativeBuild) { $publisherArgs += '--skip-native-build' }
& dotnet run --project "$engineSource/Managed/ProjectPublisher/ProjectPublisher.csproj" -c Release -- @publisherArgs
exit $LASTEXITCODE
