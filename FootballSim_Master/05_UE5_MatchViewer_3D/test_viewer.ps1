param([string]$EngineRoot = 'D:\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'FootballReplayViewer.uproject'
$reportPath = Join-Path $PSScriptRoot 'Saved\Automation'
$logPath = Join-Path $PSScriptRoot 'Saved\Stage2_2_Tests.log'
$testArguments=@(('"'+$projectPath+'"'),'-unattended','-RenderOffscreen','-NullRHI','-nosound','-nosplash','-ExecCmds="Automation RunTests FootballMatchBridge"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$reportPath+'"'),('-abslog="'+$logPath+'"'))
$testProcess=Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList $testArguments -WindowStyle Hidden -PassThru
$testProcess.WaitForExit()
if($testProcess.ExitCode -ne 0) { exit $testProcess.ExitCode }
if(-not (Select-String -LiteralPath $logPath -SimpleMatch 'tests performed.' -Quiet)) { throw 'Automation did not complete' }
if(Select-String -LiteralPath $logPath -SimpleMatch 'Result={Fail}' -Quiet) { throw 'Automation test failed' }
Select-String -LiteralPath $logPath -SimpleMatch 'Test Completed. Result=' | ForEach-Object { $_.Line }
