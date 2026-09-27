@echo off
rem Shared settings for the Tools\*.bat helpers.
rem If Unreal is installed somewhere else, set UE_ROOT (e.g. in System > Environment Variables):
rem   UE_ROOT=D:\Epic\UE_5.8
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
set "PROJECT=%PROJECT_DIR%\LootboxRecursion.uproject"
if not exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
	echo Unreal Engine not found at "%UE_ROOT%". Set the UE_ROOT environment variable.
	exit /b 1
)
exit /b 0
