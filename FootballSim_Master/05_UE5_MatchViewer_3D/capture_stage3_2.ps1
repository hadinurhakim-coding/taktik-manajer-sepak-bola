param([string]$EngineRoot='D:\Epic Games\UE_5.8', [double]$BroadcastAt=-1, [double]$CloseupAt=-1, [string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$projectFile=Join-Path $PSScriptRoot 'FootballReplayViewer.uproject'
$logs=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\00_Specs_and_Logs'))
if($OutputDirectory) { $logs=[IO.Path]::GetFullPath($OutputDirectory) }
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$arguments=@(('"'+$projectFile+'"'),'-game','-windowed','-RenderOffscreen','-unattended','-nosplash','-nosound','-NoScreenMessages','-ResX=1600','-ResY=900','-ForceRes','-NoVSync',('-Stage32CaptureDir="'+$logs+'"'),('-abslog="'+$logs+'\stage3_2_capture_runtime.log"'))
if($BroadcastAt -ge 0) { $arguments+=('-BroadcastAt='+$BroadcastAt.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if($CloseupAt -ge 0) { $arguments+=('-CloseupAt='+$CloseupAt.ToString([Globalization.CultureInfo]::InvariantCulture)) }
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$captureStarted=$process.StartTime.ToUniversalTime()
Write-Output ('CAPTURE_PID='+$process.Id)
$process.WaitForExit()
if($process.ExitCode -ne 0) { throw ('UE capture failed: '+$process.ExitCode) }
if(-not (Select-String -LiteralPath (Join-Path $logs 'stage3_2_capture_runtime.log') -SimpleMatch 'STAGE32_CAPTURE_COMPLETE' -Quiet)) { throw 'Capture did not finish successfully' }
if(Select-String -LiteralPath (Join-Path $logs 'stage3_2_capture_runtime.log') -Pattern 'Failed to compile Material|has no skeleton|missing usage flag|LogScript: Error' -Quiet) { throw 'Render validation failed; inspect runtime log' }
foreach($name in @('stage3_2_broadcast_3d.png','stage3_2_replay_closeup.png')) {
    $file=Get-Item -LiteralPath (Join-Path $logs $name)
    if($file.LastWriteTimeUtc -lt $captureStarted) { throw ('Stale screenshot: '+$name) }
    Write-Output ($file.FullName+' '+$file.Length+' bytes')
}
