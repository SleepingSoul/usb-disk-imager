@echo off
rem ---------------------------------------------------------------------------
rem Launches a locally built USB Disk Imager with the Qt libraries on PATH.
rem
rem A build tree links against Qt but keeps no copy of it, so the DLLs have to be
rem findable at startup. This resolves Qt from --qt, then QTDIR, then a qmake
rem already on PATH, then the kit the build requires under C:\Qt, then the newest
rem kit there. A deployed build that carries its own Qt DLLs is left alone.
rem
rem   run-windows.bat [--admin] [--qt <Qt kit dir>] [--build-dir <dir>] [-- app args]
rem ---------------------------------------------------------------------------
setlocal EnableDelayedExpansion

rem Resolved through a for loop so the paths this prints have no "scripts\.." in them.
for %%r in ("%~dp0..") do set "REPO_ROOT=%%~fr"
set "APP_NAME=usb-disk-imager.exe"

rem Mirrors UDI_QT_MINIMUM_VERSION in CMakeLists.txt: the version the executable was linked
rem against, so it is preferred over any newer kit that happens to be installed here.
set "QT_REQUIRED_VERSION=6.8.3"
set "QT_BIN="
set "BUILD_DIR="
set "RUN_AS_ADMIN="
set "APP_ARGS="
set "APP_ARGS_PS="
set "APP_EXE="

:parse_arguments
if "%~1"=="" goto arguments_parsed

if /i "%~1"=="--help" goto usage
if /i "%~1"=="-h" goto usage
if /i "%~1"=="/?" goto usage

if /i "%~1"=="--admin" (
    set "RUN_AS_ADMIN=1"
    shift
    goto parse_arguments
)
if /i "%~1"=="-a" (
    set "RUN_AS_ADMIN=1"
    shift
    goto parse_arguments
)
if /i "%~1"=="--qt" (
    if "%~2"=="" (
        echo [error] --qt needs the path of a Qt kit, e.g. C:\Qt\%QT_REQUIRED_VERSION%\msvc2022_64
        exit /b 2
    )
    set "QT_BIN=%~2\bin"
    shift
    shift
    goto parse_arguments
)
if /i "%~1"=="--build-dir" (
    if "%~2"=="" (
        echo [error] --build-dir needs the path of a build directory
        exit /b 2
    )
    set "BUILD_DIR=%~2"
    shift
    shift
    goto parse_arguments
)
if "%~1"=="--" (
    shift
    goto collect_app_arguments
)

call :append_app_argument "%~1"
shift
goto parse_arguments

:collect_app_arguments
if "%~1"=="" goto arguments_parsed
call :append_app_argument "%~1"
shift
goto collect_app_arguments

rem Kept in two forms: quoted for a direct call, and a comma-separated PowerShell
rem list for the elevated path, which goes through Start-Process.
:append_app_argument
set "APP_ARGS=%APP_ARGS% "%~1""
if defined APP_ARGS_PS (
    set "APP_ARGS_PS=%APP_ARGS_PS%,'%~1'"
) else (
    set "APP_ARGS_PS='%~1'"
)
exit /b 0

:arguments_parsed

rem --- Find the executable ---------------------------------------------------
if defined BUILD_DIR (
    if exist "%BUILD_DIR%\%APP_NAME%" set "APP_EXE=%BUILD_DIR%\%APP_NAME%"
) else (
    rem Release first, so a machine carrying both runs the one meant for use.
    for %%d in ("build\release" "build\Release" "build\debug" "build\Debug" "build") do (
        if not defined APP_EXE if exist "%REPO_ROOT%\%%~d\%APP_NAME%" set "APP_EXE=%REPO_ROOT%\%%~d\%APP_NAME%"
    )
    if not defined APP_EXE (
        for /r "%REPO_ROOT%\build" %%f in ("%APP_NAME%") do (
            if not defined APP_EXE if exist "%%~f" set "APP_EXE=%%~f"
        )
    )
)

if not defined APP_EXE (
    echo [error] No build of %APP_NAME% found under "%REPO_ROOT%\build".
    echo.
    echo Build it first from a Developer Command Prompt ^(or after vcvars64.bat^):
    echo   cmake --preset windows-release
    echo   cmake --build build/release
    goto failed
)

for %%f in ("%APP_EXE%") do set "APP_DIR=%%~dpf"

rem --- Find Qt, unless the build already carries it ---------------------------
rem An explicit --qt wins over everything, so a wrong one is worth reporting
rem rather than quietly replacing with a guess.
if defined QT_BIN (
    if not exist "%QT_BIN%\Qt6Core.dll" (
        echo [error] "%QT_BIN%" holds no Qt6Core.dll — is that a Qt kit directory?
        goto failed
    )
)

if not defined QT_BIN if exist "%APP_DIR%Qt6Core.dll" set "QT_BIN=deployed"
if not defined QT_BIN if exist "%APP_DIR%Qt6Cored.dll" set "QT_BIN=deployed"

if not defined QT_BIN (
    if defined QTDIR if exist "%QTDIR%\bin\Qt6Core.dll" set "QT_BIN=%QTDIR%\bin"
)

if not defined QT_BIN (
    for /f "delims=" %%q in ('where qmake.exe 2^>nul') do (
        if not defined QT_BIN if exist "%%~dpqQt6Core.dll" set "QT_BIN=%%~dpq"
    )
)

if not defined QT_BIN (
    for /f "delims=" %%k in ('dir /b /ad "C:\Qt\%QT_REQUIRED_VERSION%" 2^>nul') do (
        if not defined QT_BIN if exist "C:\Qt\%QT_REQUIRED_VERSION%\%%k\bin\Qt6Core.dll" set "QT_BIN=C:\Qt\%QT_REQUIRED_VERSION%\%%k\bin"
    )
)

if not defined QT_BIN (
    rem Descending name order puts the highest-numbered Qt first.
    for /f "delims=" %%v in ('dir /b /ad /o-n "C:\Qt\6.*" 2^>nul') do (
        for /f "delims=" %%k in ('dir /b /ad "C:\Qt\%%v" 2^>nul') do (
            if not defined QT_BIN if exist "C:\Qt\%%v\%%k\bin\Qt6Core.dll" set "QT_BIN=C:\Qt\%%v\%%k\bin"
        )
    )
)

if not defined QT_BIN (
    echo [error] Could not find the Qt libraries.
    echo         Pass --qt "C:\Qt\%QT_REQUIRED_VERSION%\msvc2022_64", or set QTDIR.
    goto failed
)

if not "%QT_BIN%"=="deployed" set "PATH=%QT_BIN%;%PATH%"

rem --- Launch ----------------------------------------------------------------
echo [ info] executable : %APP_EXE%
if "%QT_BIN%"=="deployed" (
    echo [ info] Qt         : deployed next to the executable
) else (
    echo [ info] Qt         : %QT_BIN%
    set "QT_BIN_WITHOUT_VERSION=!QT_BIN:%QT_REQUIRED_VERSION%=!"
    if "!QT_BIN_WITHOUT_VERSION!"=="!QT_BIN!" echo [ warn] cannot confirm this is Qt %QT_REQUIRED_VERSION%, the version the build needs
)

if defined RUN_AS_ADMIN (
    rem Reading and writing raw devices needs an elevated token. The elevated
    rem process inherits this one's environment, so it finds Qt on the PATH set
    rem above; only the token differs.
    echo [ info] rights     : requesting elevation
    if defined APP_ARGS_PS (
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -Verb RunAs -FilePath '%APP_EXE%' -ArgumentList @(%APP_ARGS_PS%)" || goto elevation_failed
    ) else (
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -Verb RunAs -FilePath '%APP_EXE%'" || goto elevation_failed
    )
    echo [ info] started elevated; this window can be closed
    exit /b 0
)

echo [ info] rights     : as invoked ^(elevate from inside the app, or pass --admin^)
"%APP_EXE%"%APP_ARGS%
set "APP_EXIT=%ERRORLEVEL%"

if not "%APP_EXIT%"=="0" (
    echo [ warn] exited with code %APP_EXIT%
    echo [ info] the log is under %%LOCALAPPDATA%%\Tihran Katolikian\USB Disk Imager\logs
    goto failed
)

exit /b 0

:elevation_failed
echo [error] Elevation was declined or failed.
goto failed

:usage
echo Launches a locally built USB Disk Imager with the Qt libraries on PATH.
echo.
echo   run-windows.bat [options] [-- arguments passed to the app]
echo.
echo   --admin, -a          Start elevated, which raw device access requires.
echo   --qt ^<dir^>           Qt kit to use, e.g. C:\Qt\%QT_REQUIRED_VERSION%\msvc2022_64.
echo   --build-dir ^<dir^>    Build directory to run from.
echo   --help, -h           Show this help.
echo.
echo Without options it runs the newest of build\release and build\debug, and
echo resolves Qt from QTDIR, a qmake on PATH, or C:\Qt, preferring the Qt
echo %QT_REQUIRED_VERSION% kit that the build requires.
exit /b 0

:failed
rem A double-clicked script would close before its message could be read.
echo %CMDCMDLINE% | find /i "%~nx0" >nul && pause
exit /b 1
