@echo off
REM ============================================================
REM  Build Script for C Backend Server (MSVC)
REM  Run this from Developer Command Prompt for VS
REM ============================================================

setlocal

set SRC_DIR=src
set INC_DIR=include
set OUT_DIR=build
set EXE_NAME=CBackendServer.exe

echo ============================================
echo   Building C Backend Server (MSVC)
echo ============================================
echo.

REM Check if cl.exe is available; try common paths first
set CL_PATH=
where cl.exe >nul 2>&1 && set CL_PATH=cl.exe || (
    for /f "delims=" %%p in ('dir /b /s "C:\Program Files\Microsoft Visual Studio\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" 2^>nul') do set CL_PATH=%%p
)

if "%CL_PATH%"=="" (
    echo ERROR: cl.exe not found!
    echo Please run this from a Developer Command Prompt for VS.
    pause
    exit /b 1
)

echo Using: %CL_PATH%
%CL_PATH% 2>&1 | findstr /i "version"
echo.
echo Compiling...
echo.

%CL_PATH% /W4 /O2 /MD /D_WINSOCK_DEPRECATED_NO_WARNINGS ^
    /I"%INC_DIR%" ^
    /Fe"%OUT_DIR%\%EXE_NAME%" ^
    "%SRC_DIR%\main.c" ^
    "%SRC_DIR%\logger.c" ^
    "%SRC_DIR%\json.c" ^
    "%SRC_DIR%\config.c" ^
    "%SRC_DIR%\http_parser.c" ^
    "%SRC_DIR%\http_response.c" ^
    "%SRC_DIR%\router.c" ^
    "%SRC_DIR%\thread_pool.c" ^
    "%SRC_DIR%\handlers.c" ^
    "%SRC_DIR%\server.c" ^
    /link ws2_32.lib kernel32.lib user32.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Build successful!
echo   Output: %OUT_DIR%\%EXE_NAME%
echo ============================================
echo.
echo To run: %OUT_DIR%\%EXE_NAME% --port 8080
echo.

endlocal
pause
