@echo off
rem Run the LootboxRecursion automation tests headless and print results.
setlocal
call "%~dp0ue_env.bat" || exit /b 1
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" ^
	-ExecCmds="Automation RunTests LootboxRecursion" -TestExit="Automation Test Queue Empty" ^
	-unattended -nopause -nullrhi -nosplash -stdout -FullStdOutLogOutput
