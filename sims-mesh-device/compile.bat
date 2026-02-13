@echo off
echo Building SIMS Mesh Device firmware for Heltec LoRa 32 V3...
echo.

REM Check if platformio is available
where pio >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: PlatformIO not found in PATH
    echo.
    echo Please install PlatformIO:
    echo - Via VS Code: Install PlatformIO IDE extension
    echo - Via CLI: pip install platformio
    echo.
    echo If using VS Code extension, use the PlatformIO build button instead.
    pause
    exit /b 1
)

echo Building for environment: heltec_wifi_lora_32_V3
echo.
pio run -e heltec_wifi_lora_32_V3

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Firmware compiled successfully.
    echo Ready to upload to device.
    echo.
) else (
    echo.
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
    echo.
    echo Check the errors above and fix them.
    echo.
)

pause
