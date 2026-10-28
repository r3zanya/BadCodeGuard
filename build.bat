@echo off
rem Configure, build and deploy BadCode Guard (MSVC + Qt 6).
rem
rem   build.bat [output-folder]        output folder defaults to "dist"
rem
rem Optional environment variables:
rem   QT_DIR     Qt kit folder, e.g. C:\Qt\6.8.3\msvc2022_64
rem              (default: the newest msvc2022_64 kit under C:\Qt)
rem   BUILD_DIR  build folder (default: build)
setlocal EnableDelayedExpansion
cd /d "%~dp0"

if "%QT_DIR%"=="" (
    for /d %%D in (C:\Qt\6.*) do if exist "%%D\msvc2022_64\bin\windeployqt.exe" set "QT_DIR=%%D\msvc2022_64"
)
if "%QT_DIR%"=="" (
    echo QT_DIR is not set and no Qt 6 kit "msvc2022_64" was found under C:\Qt.
    exit /b 1
)

rem Use the C++ compiler that is already on PATH, otherwise look for Visual Studio.
where cl >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -property installationPath`) do set "VSDIR=%%I"
    )
    if not defined VSDIR (
        echo Visual Studio with the C++ workload was not found.
        exit /b 1
    )
    call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)

rem CMake and Ninja from the Qt installer are used when present; otherwise those on PATH.
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%QT_DIR%\bin;%PATH%"

set "OUT=%~1"
if "%OUT%"=="" set "OUT=dist"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build"

cmake -S . -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%" || exit /b 1
cmake --build "%BUILD_DIR%" || exit /b 1

if not exist "%OUT%" mkdir "%OUT%"
copy /y "%BUILD_DIR%\BadCodeGuard.exe" "%OUT%\" >nul || exit /b 1
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%OUT%\BadCodeGuard.exe" || exit /b 1
echo Built: %OUT%\BadCodeGuard.exe
