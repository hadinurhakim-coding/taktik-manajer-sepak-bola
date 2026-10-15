$ErrorActionPreference='Stop'
$factory=Split-Path -Parent $PSScriptRoot
$runtime=Join-Path $factory 'Saved\ComfyRuntime'
foreach($folder in @('input','output','temp','user')) { New-Item -ItemType Directory -Force -Path (Join-Path $runtime $folder) | Out-Null }
$argsList=@('-s','D:\ComfyUI_windows_portable\ComfyUI\main.py','--listen','127.0.0.1','--port','8190',
    '--disable-auto-launch','--lowvram','--reserve-vram','3','--disable-pinned-memory','--disable-dynamic-vram','--disable-async-offload',
    '--offline','--cache-none','--disable-all-custom-nodes','--whitelist-custom-nodes','ComfyUI-Hunyuan3DWrapper','ComfyUI-HunyuanPaint21-LocalD',
    '--input-directory',("`""+(Join-Path $runtime 'input')+"`""),
    '--output-directory',("`""+(Join-Path $runtime 'output')+"`""),
    '--temp-directory',("`""+(Join-Path $runtime 'temp')+"`""),
    '--user-directory',("`""+(Join-Path $runtime 'user')+"`""))
$process=Start-Process -FilePath 'D:\ComfyUI_windows_portable\python_embeded\python.exe' -ArgumentList $argsList -WindowStyle Hidden -WorkingDirectory 'D:\ComfyUI_windows_portable\ComfyUI' -RedirectStandardOutput (Join-Path $runtime 'stdout.log') -RedirectStandardError (Join-Path $runtime 'stderr.log') -PassThru
@{pid=$process.Id;port=8190;output=(Join-Path $runtime 'output')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runtime 'process.json')
Write-Output "Dedicated ComfyUI PID $($process.Id), localhost:8190"
