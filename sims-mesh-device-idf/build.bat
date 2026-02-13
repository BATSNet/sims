@echo off
set ESPPORT=COM7

:: Auto-configure ESP-IDF environment if not already set
if not defined IDF_PATH (
    if exist "C:\Users\pp\esp\v5.5-rc1\esp-idf" (
        set IDF_PATH=C:\Users\pp\esp\v5.5-rc1\esp-idf
        set IDF_TOOLS_PATH=C:\Users\pp\.espressif
        set IDF_PYTHON_ENV_PATH=C:\Users\pp\.espressif\python_env\idf5.5_py3.11_env
        set "PATH=C:\Users\pp\.espressif\tools\cmake\3.30.2\bin;C:\Users\pp\.espressif\tools\ninja\1.12.1;C:\Users\pp\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;C:\Users\pp\.espressif\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64;C:\Users\pp\.espressif\python_env\idf5.5_py3.11_env\Scripts;%PATH%"
        echo Auto-configured ESP-IDF v5.5 environment
    ) else (
        echo ERROR: IDF_PATH is not set and ESP-IDF not found at default location.
        echo Run export.bat from your ESP-IDF installation.
        exit /b 1
    )
)

set IDF_PY=C:\Users\pp\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe %IDF_PATH%\tools\idf.py

echo SIMS Mesh Device - ESP-IDF Build (Port: %ESPPORT%)
if "%1"=="flash" (
    %IDF_PY% -p %ESPPORT% flash
) else if "%1"=="monitor" (
    %IDF_PY% -p %ESPPORT% monitor
) else if "%1"=="fm" (
    %IDF_PY% -p %ESPPORT% flash monitor
) else if "%1"=="clean" (
    %IDF_PY% fullclean
) else if "%1"=="menuconfig" (
    %IDF_PY% menuconfig
) else if "%1"=="size" (
    %IDF_PY% size
) else (
    %IDF_PY% build
    if %errorlevel% equ 0 (
        echo.
        echo Build OK. Commands: build.bat flash / monitor / fm / clean / menuconfig
    )
)
