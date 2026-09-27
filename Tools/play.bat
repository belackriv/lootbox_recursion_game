@echo off
rem Run the game standalone (no editor UI), windowed.
setlocal
call "%~dp0ue_env.bat" || exit /b 1
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT%" -game -windowed -ResX=1600 -ResY=900 -log
