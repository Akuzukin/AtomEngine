param([switch]$SkipNativeBuild)
$ErrorActionPreference = 'Stop'
function Inspect-Package([string]$GameExecutable) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $GameExecutable; $start.Arguments = '--inspect-package'
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $result = [pscustomobject]@{ ExitCode=$process.ExitCode; Json=$stdout.Result; Error=$stderr.Result }
    $process.Dispose(); return $result
}
$engineSource = Split-Path -Parent $PSScriptRoot
$workspaceRoot = Split-Path -Parent $engineSource
$testRoot = Join-Path $workspaceRoot ('generated/publisher-tests/' + [guid]::NewGuid().ToString('N'))
$fixtureRoot = Join-Path $testRoot 'Project'
$outputRoot = Join-Path $testRoot 'Game'
New-Item -ItemType Directory -Path "$fixtureRoot/Asset/Scenes","$fixtureRoot/Asset/Prefabs","$fixtureRoot/GameData" -Force | Out-Null
'A custom game asset outside the Asset folder' | Set-Content -LiteralPath "$fixtureRoot/GameData/Plain.txt" -Encoding UTF8
@'
<Scene version="1" name="Model dependency test">
 <Object id="model" name="Model"><Transform/><Mesh primitive="0" asset="Asset/Models/DamagedHelmet/DamagedHelmet.gltf" visible="true"/></Object>
</Scene>
'@ | Set-Content -LiteralPath "$fixtureRoot/Asset/Scenes/Main.scene" -Encoding UTF8
@'
<Prefab version="1" root="spawn"><Object id="spawn" name="Spawn"><Transform/><Audio clip="Asset/Sounds/se.mp3" volume="1" loop="false" playOnAwake="false"/></Object></Prefab>
'@ | Set-Content -LiteralPath "$fixtureRoot/Asset/Prefabs/Spawn.prefab" -Encoding UTF8
@'
<AtomProject version="1" name="Publisher test" startupScene="Asset/Scenes/Main.scene" scriptProject="" width="640" height="480">
 <Scenes><Scene path="Asset/Scenes/Main.scene"/></Scenes><Include><Asset path="Asset/Prefabs/Spawn.prefab"/><Asset path="Asset/Materials/RedPlastic.mat"/><Asset path="GameData/Plain.txt"/></Include>
</AtomProject>
'@ | Set-Content -LiteralPath "$fixtureRoot/Test.atproj" -Encoding UTF8
$publishScript = Join-Path $engineSource 'Tools/Publish-Project.ps1'
$publishArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$publishScript,'-Project',"$fixtureRoot/Test.atproj",'-Output',$outputRoot)
if ($SkipNativeBuild) { $publishArgs += '-SkipNativeBuild' }
& powershell @publishArgs
if ($LASTEXITCODE -ne 0) { throw 'Publisher dependency fixture failed' }
foreach ($file in @('game.atpak','Runtime/shared/Microsoft.NETCore.App','build-manifest.json')) {
    if (-not (Test-Path -LiteralPath (Join-Path $outputRoot $file))) { throw "Missing packaged dependency: $file" }
}
foreach ($file in @('Asset','GameData','game.atproj','Source','Managed','AtomEditor.ini')) {
    if (Test-Path -LiteralPath (Join-Path $outputRoot $file)) { throw "Editor-only content was packaged: $file" }
}
$inspection = Inspect-Package "$outputRoot/AtomGame.exe"
if ($inspection.ExitCode -ne 0) { throw ('Encrypted archive inspection failed: ' + $inspection.Error) }
$entries = $inspection.Json | ConvertFrom-Json
foreach ($asset in @('Asset/Models/DamagedHelmet/DamagedHelmet.gltf','Asset/Models/DamagedHelmet/DamagedHelmet.bin','Asset/Models/DamagedHelmet/Default_albedo.jpg','Asset/Prefabs/Spawn.prefab','Asset/Sounds/se.mp3','Asset/Materials/RedPlastic.mat','GameData/Plain.txt','__runtime/startup.json')) {
    if ($asset -notin $entries) { throw "Missing encrypted asset: $asset" }
}
foreach ($asset in @('Asset/Editor.xml','Asset/Shaders/ImGuiPresentPS.hlsl','Asset/Shaders/SelectionOutline.hlsl','Asset/Shaders/GridPS.hlsl')) {
    if ($asset -in $entries) { throw "Editor content in encrypted archive: $asset" }
}
if (([Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes("$outputRoot/game.atpak"))).Contains('Asset/Scenes/Main.scene')) { throw 'Asset index was not encrypted' }
$gameProcess = Start-Process -FilePath "$outputRoot/AtomGame.exe" -ArgumentList '--smoke-test' -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
if (-not $gameProcess.WaitForExit(60000)) { throw 'Published model test timed out' }
if ($gameProcess.ExitCode -ne 0) { throw 'Published model runtime failed' }
if (-not ([IO.File]::ReadAllText("$outputRoot/logs/AtomEngine.log")).Contains('Packaged prefab, material and audio memory loading passed')) { throw 'Prefab/material/audio package smoke did not run' }
if (([IO.File]::ReadAllText("$outputRoot/logs/AtomEngine.log")).Contains('Texture load failed')) { throw 'Packaged model used fallback textures' }
# Corrupted archives and missing runtime data must fail before normal startup.
$archiveBytes = [IO.File]::ReadAllBytes("$outputRoot/game.atpak")
try {
    $damagedBytes = $archiveBytes.Clone(); $damagedBytes[$damagedBytes.Length - 1] = $damagedBytes[$damagedBytes.Length - 1] -bxor 1
    [IO.File]::WriteAllBytes("$outputRoot/game.atpak", $damagedBytes)
    $inspection = Inspect-Package "$outputRoot/AtomGame.exe"
    if ($inspection.ExitCode -eq 0 -or -not $inspection.Error.Contains('integrity check failed')) { throw 'Tampered archive was accepted or failed for an unrelated reason' }
} finally { [IO.File]::WriteAllBytes("$outputRoot/game.atpak", $archiveBytes) }
$executableBytes = [IO.File]::ReadAllBytes("$outputRoot/AtomGame.exe")
try {
    $wrongKeyBytes = $executableBytes.Clone(); $keyOffset = $wrongKeyBytes.Length - 64; $wrongKeyBytes[$keyOffset] = $wrongKeyBytes[$keyOffset] -bxor 1
    [IO.File]::WriteAllBytes("$outputRoot/AtomGame.exe", $wrongKeyBytes)
    $inspection = Inspect-Package "$outputRoot/AtomGame.exe"
    if ($inspection.ExitCode -eq 0 -or -not $inspection.Error.Contains('authentication/decryption failed')) { throw 'Wrong package key was accepted' }
} finally { [IO.File]::WriteAllBytes("$outputRoot/AtomGame.exe", $executableBytes) }
$heldArchive = "$outputRoot/game.atpak.test-held"
try {
    Move-Item -LiteralPath "$outputRoot/game.atpak" -Destination $heldArchive
    $inspection = Inspect-Package "$outputRoot/AtomGame.exe"
    if ($inspection.ExitCode -eq 0 -or -not $inspection.Error.Contains('Missing game.atpak')) { throw 'Game accepted a missing archive' }
} finally { if (Test-Path -LiteralPath $heldArchive) { Move-Item -LiteralPath $heldArchive -Destination "$outputRoot/game.atpak" } }
$oldManifest = (Get-FileHash -LiteralPath "$outputRoot/build-manifest.json" -Algorithm SHA256).Hash
$oldArchive = (Get-FileHash -LiteralPath "$outputRoot.zip" -Algorithm SHA256).Hash
# Failed validation must preserve the previous publish and must not block on a dialog.
$brokenProjectText = [IO.File]::ReadAllText("$fixtureRoot/Test.atproj").Replace('width="640"','width="1"')
if (-not $brokenProjectText.Contains('width="1"')) { throw 'Invalid-window fixture was not created' }
[IO.File]::WriteAllText("$fixtureRoot/Broken.atproj", $brokenProjectText, [Text.UTF8Encoding]::new($false))
$failedArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$publishScript,'-Project',"$fixtureRoot/Broken.atproj",'-Output',$outputRoot,'-SkipNativeBuild')
& powershell @failedArgs
if ($LASTEXITCODE -eq 0) { throw 'Invalid window size was published' }
if ((Get-FileHash -LiteralPath "$outputRoot/build-manifest.json" -Algorithm SHA256).Hash -ne $oldManifest -or (Get-FileHash -LiteralPath "$outputRoot.zip" -Algorithm SHA256).Hash -ne $oldArchive) { throw 'Failed publish changed previous output' }
# A locked ZIP must roll back the directory swap as well.
$archiveLock = [IO.File]::Open("$outputRoot.zip", [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $publishScript -Project "$fixtureRoot/Test.atproj" -Output $outputRoot -SkipNativeBuild
    if ($LASTEXITCODE -eq 0) { throw 'Publish overwrote a locked archive' }
} finally { $archiveLock.Dispose() }
if ((Get-FileHash -LiteralPath "$outputRoot/build-manifest.json" -Algorithm SHA256).Hash -ne $oldManifest -or (Get-FileHash -LiteralPath "$outputRoot.zip" -Algorithm SHA256).Hash -ne $oldArchive) { throw 'Locked archive publish changed previous output' }
$badOutput = Join-Path $testRoot 'NotAnAtomOutput'
New-Item -ItemType Directory -Path $badOutput | Out-Null
'user data' | Set-Content -LiteralPath "$badOutput/keep.txt"
& powershell -NoProfile -ExecutionPolicy Bypass -File $publishScript -Project "$fixtureRoot/Test.atproj" -Output $badOutput -SkipNativeBuild
if ($LASTEXITCODE -eq 0 -or (Get-Content -LiteralPath "$badOutput/keep.txt") -ne 'user data') { throw 'Publisher overwrote an unrelated directory' }
Write-Output 'PASS encrypted index/assets, real model textures, prefab/material/audio memory loading, no project dependency, tamper/wrong-key/missing-package rejection, failed-publish rollback and output protection'
Write-Output "Fixture: $testRoot"
