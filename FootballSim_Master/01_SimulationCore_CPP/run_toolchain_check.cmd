@echo off
setlocal
call "%~dp0activate_toolchain.cmd"
if errorlevel 1 exit /b %errorlevel%
cmake --version
if errorlevel 1 exit /b %errorlevel%
cmake -S "%~dp0tests" -B "%~dp0build\toolchain_check" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0build\toolchain_check" --config Release
if errorlevel 1 exit /b %errorlevel%
"%~dp0build\toolchain_check\Release\toolchain_check.exe"
exit /b %errorlevel%
