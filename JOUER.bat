@echo off
setlocal
set "METAL_EDITOR=C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%METAL_EDITOR%" (
    echo Unreal Engine 5.7 est introuvable a son emplacement habituel.
    pause
    exit /b 1
)
start "MEtAL" "%METAL_EDITOR%" "%~dp0MEtAL.uproject" -game -windowed -ResX=1280 -ResY=720 -MetalWelcome
