@echo off
rem Open the project in the Unreal Editor (build first with build.bat).
setlocal
call "%~dp0ue_env.bat" || exit /b 1
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT%"
