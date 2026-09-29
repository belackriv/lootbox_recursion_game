@echo off
rem Build the hook materials (docs/MATERIALS.md) headless with Tools\unreal\make_materials.py.
rem Close the editor first. Usage: materials.bat              (all recipes)
rem                                materials.bat M_SeeThrough (just that one)
setlocal
call "%~dp0ue_env.bat" || exit /b 1
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" ^
	-run=pythonscript -script="%~dp0unreal\make_materials.py %*" ^
	-unattended -nopause -nosplash -stdout -FullStdOutLogOutput
