@echo off
setlocal
if not defined FSIM_UE_ROOT set "FSIM_UE_ROOT=D:\Epic Games\UE_5.8"
if not exist "%~dp0Binaries\Win64\UnrealEditor-FootballReplayViewer.dll" (
  echo Build first: powershell -File "%~dp0build_viewer.ps1"
  pause
  exit /b 1
)
start "FootballSim Replay Viewer" "%FSIM_UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0FootballReplayViewer.uproject" -game -windowed -ResX=1440 -ResY=900 -nosplash %*
