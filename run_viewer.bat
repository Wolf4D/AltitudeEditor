@echo off
setlocal
cd /d "%~dp0"
set PATH=%~dp0build;C:\Qt5\5.15.2\mingw81_32\bin;C:\Qt5\Tools\mingw810_32\bin;%PATH%
start "" "%~dp0build\AltitudeEditor.exe" %*
