@echo off
rem Compile the game module for the editor (Development Editor, Win64).
setlocal
call "%~dp0ue_env.bat" || exit /b 1
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" LootboxRecursionEditor Win64 Development -Project="%PROJECT%" -WaitMutex
