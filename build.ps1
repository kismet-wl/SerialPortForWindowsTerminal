param(
    [Alias("h", "?")]
    [switch]$help,
    [Alias("win32")]
    [switch]$x86,
    [switch]$x64,
    [Alias("d", "debug")]
    [switch]$dbg,
    [Alias("r", "release")]
    [switch]$rel,
    [Alias("a")]
    [switch]$all
)

# 字符编码设置：确保中文在控制台正确显示
try { chcp.com 65001 > $null } catch {}
$utf8 = [System.Text.Encoding]::UTF8
$OutputEncoding = $utf8           # 外部进程与管道编码
[Console]::OutputEncoding = $utf8 # 控制台输出编码
[Console]::InputEncoding  = $utf8 # 控制台输入编码

# Normalize unmatched args to support forms like: x64 debug all
$normalized = @()
foreach ($arg in $args) {
    if ($null -ne $arg) { $normalized += ($arg.ToString()).ToLowerInvariant() }
}
foreach ($a in $normalized) {
    switch -regex ($a) {
        '^(?:-)?(?:help|h|\?)$'     { $help = $true }
        '^(?:-)?(?:all|a)$'          { $all = $true }
        '^(?:-)?(?:x64)$'            { $x64 = $true }
        '^(?:-)?(?:x86|win32)$'      { $x86 = $true }
        '^(?:-)?(?:debug|dbg|d)$'    { $dbg = $true }
        '^(?:-)?(?:release|rel|r)$'  { $rel = $true }
    }
}

# Defaults
$arch = "all"
$cfg  = "all"

# 帮助优先显示（无需安装 VS 也可查看）
if ($help) {
    Write-Host "用法: build.ps1 [选项]"
    Write-Host ""
    Write-Host "选项:"
    Write-Host "  -help, -h, -?        显示本帮助信息"
    Write-Host "  -x86, -win32         构建 x86 架构"
    Write-Host "  -x64                 构建 x64 架构"
    Write-Host "  -debug, -dbg, -d     构建 Debug 版本"
    Write-Host "  -release, -rel, -r   构建 Release 版本"
    Write-Host "  -all, -a             构建全部（架构 + 配置）"
    Write-Host "  默认: 构建全部"
    exit 0
}

# Parse switches into arch/cfg
if ($x86) { $arch = "x86" } elseif ($x64) { $arch = "x64" }
if ($dbg) { $cfg  = "debug" } elseif ($rel) { $cfg = "release" }
if ($all) { $arch = "all"; $cfg = "all" }

# Locate Visual Studio
$VS_PATH = $null
$VS_VERSION = $null

$VS2022_PATHS = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community"
)
foreach ($path in $VS2022_PATHS) {
    if (Test-Path "$path\Common7\IDE\devenv.com") { $VS_PATH = $path; $VS_VERSION = "2022"; break }
}

if (-not $VS_PATH) {
    $VS2019_PATHS = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community"
    )
    foreach ($path in $VS2019_PATHS) {
        if (Test-Path "$path\Common7\IDE\devenv.com") { $VS_PATH = $path; $VS_VERSION = "2019"; break }
    }
}

if (-not $VS_PATH) {
    Write-Error "错误: 未找到 Visual Studio 2019 或 2022"
    exit 1
}

Write-Host "已找到 Visual Studio $($VS_VERSION) ，路径: $VS_PATH"

# Import VS dev environment
$VsDevCmd = Join-Path $VS_PATH "Common7\Tools\VsDevCmd.bat"
if (Test-Path $VsDevCmd) {
    $Command = "`"$VsDevCmd`" & set"
    cmd /c $Command | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] }
    }
}

# 确保 bin 目录存在
if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Path "bin" | Out-Null }

function Invoke-ProjectBuild {
    param(
        [string]$Platform,
        [string]$Configuration
    )
    Write-Host "正在构建 $Platform $Configuration..."
    msbuild SerialForWindowsTerminal.vcxproj /p:Configuration=$Configuration /p:Platform=$Platform /t:Rebuild
    if ($LASTEXITCODE -ne 0) { Write-Host "构建失败: $Platform $Configuration"; return $false }
    return $true
}

# Expand lists
$archList = if ($arch -eq "all") { @("Win32","x64") } elseif ($arch -eq "x86") { @("Win32") } else { @("x64") }
$cfgList  = if ($cfg  -eq "all") { @("Debug","Release") } elseif ($cfg -eq "debug") { @("Debug") } else { @("Release") }

$buildFailed = $false
foreach ($p in $archList) {
    foreach ($c in $cfgList) {
        if (-not (Invoke-ProjectBuild $p $c)) { $buildFailed = $true }
    }
}

if ($buildFailed) { Write-Error "部分目标构建失败"; exit 1 }

Write-Host "所有指定目标构建成功"
Write-Host "构建产物位于 Debug/Release 与 x64/Debug/Release 目录"

# 复制产物到 bin 目录
if ($arch -eq "all") {
    Copy-Item "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" -ErrorAction SilentlyContinue
    Copy-Item "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" -ErrorAction SilentlyContinue
    Copy-Item "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" -ErrorAction SilentlyContinue
} elseif ($arch -eq "x86") {
    if ($cfg -eq "all" -or $cfg -eq "debug") { Copy-Item "Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_debug.exe" -ErrorAction SilentlyContinue }
    if ($cfg -eq "all" -or $cfg -eq "release") { Copy-Item "Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x86_release.exe" -ErrorAction SilentlyContinue }
} elseif ($arch -eq "x64") {
    if ($cfg -eq "all" -or $cfg -eq "debug") { Copy-Item "x64\Debug\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_debug.exe" -ErrorAction SilentlyContinue }
    if ($cfg -eq "all" -or $cfg -eq "release") { Copy-Item "x64\Release\SerialForWindowsTerminal.exe" "bin\SerialForWindowsTerminal_x64_release.exe" -ErrorAction SilentlyContinue }
}

Write-Host "构建产物已复制到 bin 目录"
