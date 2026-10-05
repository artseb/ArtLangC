@echo off
setlocal EnableDelayedExpansion

:: ============================================================
:: ART bootstrap for Windows
::
:: Installs MSYS2 + GCC + make if they aren't already present,
:: then builds ART and runs the test suite.
::
:: Requires administrator rights (MSYS2 installs to C:\msys64).
:: First run downloads around 600 MB total across the MSYS2
:: installer and the toolchain packages. Re-runs are fast —
:: everything is skipped if already present.
:: ============================================================

set "MSYS2_ROOT=C:\msys64"
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

echo.
echo ============================================================
echo   ART bootstrap
echo ============================================================
echo.

:: --- Admin check ---
net session >nul 2>&1
if errorlevel 1 (
    echo [x] This script needs administrator rights.
    echo     Right-click the file, choose "Run as administrator".
    echo.
    pause
    exit /b 1
)

:: --- 1. MSYS2 ---
if exist "%MSYS2_ROOT%\usr\bin\bash.exe" goto have_msys2

echo [1/4] Downloading MSYS2...
set "MSYS2_EXE=%TEMP%\msys2-installer.exe"
powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://github.com/msys2/msys2-installer/releases/latest/download/msys2-x86_64-latest.exe' -OutFile '%MSYS2_EXE%'"

if not exist "%MSYS2_EXE%" (
    echo [x] Download failed. Check your internet connection.
    echo     If the link is broken, download the installer manually
    echo     from https://www.msys2.org and run it, then re-run this.
    pause
    exit /b 1
)

echo      Installing to %MSYS2_ROOT% (silent, may take a minute)...
"%MSYS2_EXE%" /S /D=%MSYS2_ROOT%

:: NSIS returns immediately; poll for the binary to appear.
set /a WAIT=0
:wait_msys2
if exist "%MSYS2_ROOT%\usr\bin\bash.exe" goto msys2_ready
timeout /t 3 /nobreak >nul
set /a WAIT+=1
if %WAIT% LSS 60 goto wait_msys2
echo [x] MSYS2 install timed out after 3 minutes.
pause
exit /b 1

:msys2_ready
del "%MSYS2_EXE%" 2>nul
echo      Done.
goto after_msys2

:have_msys2
echo [1/4] MSYS2 already installed at %MSYS2_ROOT%

:after_msys2

:: --- 2. Toolchain ---
if exist "%MSYS2_ROOT%\ucrt64\bin\gcc.exe" goto have_toolchain

echo [2/4] Installing GCC and make (a few minutes, ~500 MB)...
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed make mingw-w64-ucrt-x86_64-gcc"
if errorlevel 1 (
    echo [x] pacman failed. Check the output above.
    pause
    exit /b 1
)
goto after_toolchain

:have_toolchain
echo [2/4] GCC toolchain already installed

:after_toolchain

:: --- 3. Convert the script's directory to an MSYS2 path ---
:: D:\ArtLangRemake  ->  /d/ArtLangRemake
set "WIN_DIR=%SCRIPT_DIR:\=/%"
set "DRIVE=%WIN_DIR:~0,1%"
set "DRIVE_LOWER="
for %%a in (a b c d e f g h i j k l m n o p q r s t u v w x y z) do (
    if /i "%DRIVE%"=="%%a" set "DRIVE_LOWER=%%a"
)
set "MSYS_DIR=/%DRIVE_LOWER%%WIN_DIR:~2%"
echo      Project directory: %MSYS_DIR%

:: --- 4. Build ---
echo [3/4] Building ART...
set "MSYSTEM=UCRT64"
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "cd '%MSYS_DIR%' && make clean && make"
if errorlevel 1 (
    echo [x] Build failed. See errors above.
    pause
    exit /b 1
)

:: --- 5. Test ---
echo [4/4] Running the test suite...
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "cd '%MSYS_DIR%' && make test"
if errorlevel 1 (
    echo [x] Some tests failed. Report at the repo issue tracker.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   ART built successfully.
echo.
echo   Binary:  %SCRIPT_DIR%\bin\art.exe
echo.
echo   Try it:
echo     bin\art.exe examples\warmup.art
echo     bin\art.exe examples\vector3.art
echo     bin\art.exe
echo ============================================================
echo.
pause
endlocal
