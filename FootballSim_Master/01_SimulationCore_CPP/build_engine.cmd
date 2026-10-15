@echo off
setlocal
call "%~dp0activate_toolchain.cmd"
if errorlevel 1 exit /b %errorlevel%
cmake -S "%~dp0." -B "%~dp0build\engine" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0build\engine" --config Release
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir "%~dp0build\engine" -C Release --output-on-failure
exit /b %errorlevel%
