param([string]$EngineRoot = 'D:\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'FootballReplayViewer.uproject'
$savedDir = Join-Path $PSScriptRoot 'Saved'
New-Item -ItemType Directory -Force -Path $savedDir | Out-Null
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') FootballReplayViewerEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=8 "-Log=$savedDir\Stage2_2_UBT.log"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output 'FSIM_VIEWER_BUILD_OK'
