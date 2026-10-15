param([string]$EngineRoot = 'D:\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'FootballReplayViewer.uproject'
$scriptPath = Join-Path $PSScriptRoot 'Scripts\setup_assets.py'
$logPath = Join-Path $PSScriptRoot 'Saved\Stage2_2_Assets.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $projectPath -run=pythonscript "-script=$scriptPath" -unattended -NullRHI -nosound -nosplash "-abslog=$logPath"
exit $LASTEXITCODE
