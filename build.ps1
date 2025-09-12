param(
    [Parameter(Position=0)]
    [string[]]$Args,
    
    [Alias("h", "?")]
    [switch]$help,
    [Alias("win32")]
    [switch]$x86,
    [switch]$x64,
    [Alias("d")]
    [switch]$dbg,
    [Alias("r")]
    [switch]$rel,
    [switch]$all
)

# 设置输出编码为 UTF-8
$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# 查找 Visual Studio 安装路径
$VS_PATH = $null
$VS_VERSION = $null

# 首先检查 VS2022
$VS2022_PATHS = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community"
)

foreach ($path in $VS2022_PATHS) {
    if (Test-Path "$path\Common7\IDE\devenv.com") {
        $VS_PATH = $path
        $VS_VERSION = "2022"
        break
    }
}

# 如果没找到VS2022，检查VS2019
if (-not $VS_PATH) {
    $VS2019_PATHS = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community"
    )

    foreach ($path in $VS2019_PATHS) {
        if (Test-Path "$path\Common7\IDE\devenv.com") {
            $VS_PATH = $path
            $VS_VERSION = "2019"
            break
        }
    }
}

if (-not $VS_PATH) {
    Write-Error "错误: 未找到 Visual Studio 2019 或 2022"
    exit 1
}

Write-Host "找到 Visual Studio $VS_VERSION 在: $VS_PATH"

# 导入环境变量
$VsDevCmd = Join-Path $VS_PATH "Common7\Tools\VsDevCmd.bat"
if (Test-Path $VsDevCmd) {
    # 使用 cmd.exe 运行 VsDevCmd.bat 并导入其环境变量
    $Command = "`"$VsDevCmd`" & set"
    cmd /c $Command | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            $varName = $matches[1]
            $varValue = $matches[2]
            Set-Item "env:$varName" $varValue
        }
    }
}

# 参数解析
$arch = "all"
$cfg = "all"

# 检查是否需要显示帮助
if ($help -or $h -or $Args -contains "help" -or $Args -contains "/?" -or $Args -contains "-help") {
    Write-Host "用法: build.ps1 [选项]"
    Write-Host ""
    Write-Host "选项:"
    Write-Host "  -help, -h, -?      显示本帮助信息"
    Write-Host "  -x86, -win32       仅编译 x86 架构"
    Write-Host "  -x64               仅编译 x64 架构"
    Write-Host "  -dbg, -d          仅编译 Debug 版本"
    Write-Host "  -rel, -r          仅编译 Release 版本"
    Write-Host "  -all              编译全部架构和类型"
    Write-Host "  默认全部编译"
    exit 0
}

# 处理架构参数
if ($x86) { $arch = "x86" }
elseif ($x64) { $arch = "x64" }

# 处理配置参数
if ($dbg) { $cfg = "debug" }
elseif ($rel) { $cfg = "release" }

# 处理全部编译参数
if ($all) { $arch = "all"; $cfg = "all" }

# 创建输出目录
if (-not (Test-Path "bin")) {
    New-Item -ItemType Directory -Path "bin" | Out-Null
}

Write-Host "开始构建..."

function Build-Project {
    param (
        [string]$Platform,
        [string]$Configuration
    )
    Write-Host "正在构建 $Platform $Configuration 版本..."
    $result = msbuild SerialForWindowsTerminal.vcxproj /p:Configuration=$Configuration /p:Platform=$Platform /t:Rebuild
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$Platform $Configuration 构建失败"
        return $false
    }
    return $true
}

# 构建逻辑
$archList = @()
if ($arch -eq "all") {
    $archList = @("Win32", "x64")
} elseif ($arch -eq "x86") {
    $archList = @("Win32")
} elseif ($arch -eq "x64") {
    $archList = @("x64")
}

$cfgList = @()
if ($cfg -eq "all") {
    $cfgList = @("Debug", "Release")
} elseif ($cfg -eq "debug") {
    $cfgList = @("Debug")
} elseif ($cfg -eq "release") {
    $cfgList = @("Release")
}

$buildFailed = $false
foreach ($p in $archList) {
    foreach ($c in $cfgList) {
        if (-not (Build-Project $p $c)) {
            $buildFailed = $true
        }
    }
}

if ($buildFailed) {
    Write-Error "有部分版本构建失败"
    exit 1
}

Write-Host "所有指定版本构建完成！"
Write-Host "构建文件在 Debug/Release 和 x64\Debug/Release 目录中"

# 复制编译结果到 bin 目录
if ($arch -eq "all") {
    Copy-Item "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" -ErrorAction SilentlyContinue
    Copy-Item "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" -ErrorAction SilentlyContinue
} elseif ($arch -eq "x86") {
    Copy-Item "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" -ErrorAction SilentlyContinue
} elseif ($arch -eq "x64") {
    Copy-Item "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" -ErrorAction SilentlyContinue
}

Write-Host "构建文件已复制到 bin 目录"