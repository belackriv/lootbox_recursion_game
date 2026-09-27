@echo off
rem Generate compile_commands.json in the project root so clangd (Zed, VS Code, etc.)
rem understands the Unreal build flags and include paths. Re-run after adding new files.
setlocal
call "%~dp0ue_env.bat" || exit /b 1
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" -mode=GenerateClangDatabase -Project="%PROJECT%" -OutputDir="%PROJECT_DIR%" LootboxRecursionEditor Win64 Development
