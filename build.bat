@echo off
REM 确保中文正常显示（UTF-8）
chcp 65001>nul
setlocal EnableDelayedExpansion

REM 参数解析
set "ARCH=all"
set "CFG=all"
set "SHOW_HELP=0"

for %%A in (%*) do (
    REM 帮助
    if /I "%%A"=="-help" set "SHOW_HELP=1"
    if /I "%%A"=="-h" set "SHOW_HELP=1"
    if /I "%%A"=="/?" set "SHOW_HELP=1"

    REM 架构
    if /I "%%A"=="-x86" set "ARCH=x86"
    if /I "%%A"=="-win32" set "ARCH=x86"
    if /I "%%A"=="x86" set "ARCH=x86"
    if /I "%%A"=="win32" set "ARCH=x86"
    if /I "%%A"=="-x64" set "ARCH=x64"
    if /I "%%A"=="x64" set "ARCH=x64"

    REM 配置
    if /I "%%A"=="-debug" set "CFG=debug"
    if /I "%%A"=="-dbg" set "CFG=debug"
    if /I "%%A"=="-d" set "CFG=debug"
    if /I "%%A"=="debug" set "CFG=debug"
    if /I "%%A"=="dbg" set "CFG=debug"
    if /I "%%A"=="-release" set "CFG=release"
    if /I "%%A"=="-rel" set "CFG=release"
    if /I "%%A"=="-r" set "CFG=release"
    if /I "%%A"=="release" set "CFG=release"
    if /I "%%A"=="rel" set "CFG=release"

    REM 全部
    if /I "%%A"=="-all" (set "ARCH=all" & set "CFG=all")
    if /I "%%A"=="all" (set "ARCH=all" & set "CFG=all")
    if /I "%%A"=="-a" (set "ARCH=all" & set "CFG=all")
)

if %SHOW_HELP% EQU 1 (
    echo.
    echo 用法: build.bat [选项]
    echo.
    echo 选项:
    echo   -help, -h, /?        显示本帮助信息
    echo   -x86, -win32         构建 x86 架构
    echo   -x64                 构建 x64 架构
    echo   -debug, -dbg, -d     构建 Debug 版本
    echo   -release, -rel, -r   构建 Release 版本
    echo   -all, -a             构建全部（架构 ^+ 配置）
    echo   默认: 构建全部
    echo.
    exit /b 0
)

REM 确保输出目录
if not exist "bin" mkdir bin

REM 查找 Visual Studio 安装路径
set "VS_PATH="
set "VS_VERSION="

REM 优先 VS2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.com" (
    set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    set "VS_VERSION=2022"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.com" (
    set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    set "VS_VERSION=2022"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" (
    set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    set "VS_VERSION=2022"
)

REM 若无 VS2022，检查 VS2019
if "%VS_PATH%"=="" (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\Common7\IDE\devenv.com" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional"
        set "VS_VERSION=2019"
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\devenv.com" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
        set "VS_VERSION=2019"
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
        set "VS_VERSION=2019"
    )
)

if "%VS_PATH%"=="" (
    echo.
    echo 错误: 未找到 Visual Studio 2019 或 2022
    echo.
    exit /b 1
)

echo 已找到 Visual Studio %VS_VERSION% ，路径: %VS_PATH%
echo.

REM 加载开发者环境
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat"

REM 再次确保输出目录
if not exist "bin" mkdir bin

REM 构建矩阵
set "BUILD_OK=1"

if "%ARCH%"=="all" (
    set "ARCHLIST=Win32 x64"
) else if "%ARCH%"=="x86" (
    set "ARCHLIST=Win32"
) else if "%ARCH%"=="x64" (
    set "ARCHLIST=x64"
)

if "%CFG%"=="all" (
    set "CFGLIST=Debug Release"
) else if "%CFG%"=="debug" (
    set "CFGLIST=Debug"
) else if "%CFG%"=="release" (
    set "CFGLIST=Release"
)

for %%P in (%ARCHLIST%) do (
    for %%C in (%CFGLIST%) do (
        echo 正在构建 %%P %%C...
        msbuild SerialForWindowsTerminal.vcxproj /p:Configuration=%%C /p:Platform=%%P /t:Rebuild
        if errorlevel 1 (
            echo 构建失败: %%P %%C
            set "BUILD_OK=0"
        )
    )
)

if "%BUILD_OK%"=="0" (
    echo 部分目标构建失败
    exit /b 1
)

echo 所有指定目标构建成功
echo 构建产物位于 Debug/Release 与 x64/Debug/Release 目录

REM 复制产物到 bin 目录
if "%ARCH%"=="all" (
    copy "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" >nul
    copy "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" >nul
    copy "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" >nul
    copy "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" >nul
) else if "%ARCH%"=="x86" (
    if "%CFG%"=="all" (
        copy "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" >nul
        copy "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" >nul
    ) else if "%CFG%"=="debug" (
        copy "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" >nul
    ) else if "%CFG%"=="release" (
        copy "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" >nul
    )
) else if "%ARCH%"=="x64" (
    if "%CFG%"=="all" (
        copy "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" >nul
        copy "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" >nul
    ) else if "%CFG%"=="debug" (
        copy "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" >nul
    ) else if "%CFG%"=="release" (
        copy "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" >nul
    )
)

echo 构建产物已复制到 bin 目录

endlocal


