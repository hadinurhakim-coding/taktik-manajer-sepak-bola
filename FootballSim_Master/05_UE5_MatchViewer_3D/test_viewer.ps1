param([string]$EngineRoot = 'D:\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'FootballReplayViewer.uproject'
$reportPath = Join-Path $PSScriptRoot 'Saved\Automation'
$logPath = Join-Path $PSScriptRoot 'Saved\Stage2_2_Tests.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $projectPath -unattended -NullRHI -nosound -nosplash '-ExecCmds=Automation RunTests FootballMatchBridge' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath" "-abslog=$logPath"
exit $LASTEXITCODE
