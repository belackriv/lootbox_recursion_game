@echo off
rem Shared settings for the Tools\*.bat helpers.
rem Finds Unreal Engine in this order:
rem   1. the UE_ROOT environment variable, if set (e.g. UE_ROOT=D:\Epic\UE_5.8)
rem   2. the Epic Launcher's registry entry for UE_VERSION
rem   3. C:\Program Files\Epic Games\UE_<UE_VERSION>
if not defined UE_VERSION set "UE_VERSION=5.8"

if not defined UE_ROOT (
	for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%UE_VERSION%" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
)
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_%UE_VERSION%"

for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
set "PROJECT=%PROJECT_DIR%\LootboxRecursion.uproject"

if not exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
	echo Unreal Engine %UE_VERSION% not found at "%UE_ROOT%".
	echo Set the UE_ROOT environment variable to your engine folder, e.g.
	echo     setx UE_ROOT "D:\Epic Games\UE_%UE_VERSION%"
	echo then restart Zed / your terminal.
	exit /b 1
)
echo Using Unreal Engine at "%UE_ROOT%"
exit /b 0
