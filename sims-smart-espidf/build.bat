@echo off
REM Build script for SIMS-SMART ESP-IDF project
REM Device port: COM8

set ESPPORT=COM8

echo ========================================
echo SIMS-SMART ESP-IDF Build
echo Device port: %ESPPORT%
echo ========================================
echo.

REM Check if IDF_PATH is set
if not defined IDF_PATH (
    echo ERROR: IDF_PATH is not set
    echo Please run: export.bat from ESP-IDF installation
    exit /b 1
)

echo ESP-IDF Path: %IDF_PATH%
echo.

REM Set target (only needed once)
echo Setting target to ESP32-S3...
idf.py set-target esp32s3

REM Build the project
echo.
echo Building project...
idf.py build

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo Build completed successfully!
    echo ========================================
    echo.
    echo To flash:           idf.py -p COM8 flash
    echo To monitor:         idf.py -p COM8 monitor
    echo To flash + monitor: idf.py -p COM8 flash monitor
) else (
    echo.
    echo ========================================
    echo Build failed!
    echo ========================================
    exit /b 1
)
