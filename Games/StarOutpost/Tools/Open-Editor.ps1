param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
$taskProjectRoot=Split-Path -Parent $PSScriptRoot
$taskRepoRoot=[IO.Path]::GetFullPath((Join-Path $taskProjectRoot '../..'))
$taskEditorRoot=Join-Path $taskRepoRoot "generated/outputs/x64/$Configuration"
$taskComponentEditorRoot=Join-Path $taskRepoRoot 'generated/components-editor'
$taskComponentEditorExe=Join-Path $taskComponentEditorRoot 'AtomEngine.exe'
if(Test-Path -LiteralPath $taskComponentEditorExe){
    $taskStandardEditor=Join-Path $taskEditorRoot 'AtomEngine.exe'
    if((-not (Test-Path -LiteralPath $taskStandardEditor)) -or ((Get-Item -LiteralPath $taskComponentEditorExe).LastWriteTime -gt (Get-Item -LiteralPath $taskStandardEditor).LastWriteTime)){$taskEditorRoot=$taskComponentEditorRoot}
}
$taskEditorExe=Join-Path $taskEditorRoot 'AtomEngine.exe' 
$taskParticleEditorRoot=Join-Path $taskRepoRoot 'generated/particles-v2-editor'
$taskParticleEditorExe=Join-Path $taskParticleEditorRoot 'AtomEngine.exe'
if((Test-Path -LiteralPath $taskParticleEditorExe) -and ((-not (Test-Path -LiteralPath $taskEditorExe)) -or ((Get-Item -LiteralPath $taskParticleEditorExe).LastWriteTime -gt (Get-Item -LiteralPath $taskEditorExe).LastWriteTime))){$taskEditorRoot=$taskParticleEditorRoot;$taskEditorExe=$taskParticleEditorExe}
if (-not (Test-Path -LiteralPath $taskEditorExe)) {throw 'Build Engine/AtomEngine.sln before opening this project.'}
foreach($taskCandidateRoot in @((Join-Path $taskRepoRoot 'generated/particle-assets-editor'))){
    $taskCandidateExe=Join-Path $taskCandidateRoot 'AtomEngine.exe'
    if((Test-Path -LiteralPath $taskCandidateExe) -and ((Get-Item -LiteralPath $taskCandidateExe).LastWriteTime -gt (Get-Item -LiteralPath $taskEditorExe).LastWriteTime)){$taskEditorRoot=$taskCandidateRoot;$taskEditorExe=$taskCandidateExe}
}
# Engine ScriptPath resolves relative assemblies next to the executable, matching Build scripts.
& dotnet build (Join-Path $taskProjectRoot 'Managed/GameScripts/GameScripts.csproj') -c $Configuration -o (Join-Path $taskEditorRoot 'Scripts') --nologo
if ($LASTEXITCODE -ne 0) {throw 'Game script build failed.'}
Start-Process -FilePath $taskEditorExe -ArgumentList '--project',('"'+(Join-Path $taskProjectRoot 'StarOutpost.atproj')+'"') -WindowStyle Normal
