# =============================================================================
# build-msvc.ps1 -- Build gguf-tools with MSVC on Windows
#
# Usage:
#   .\build-msvc.ps1                          # show this help
#   .\build-msvc.ps1 build [release|debug] [x86|x64]
#   .\build-msvc.ps1 clean
#
# Artifacts: build\<arch>\<config>\{exe,obj,pdb,ilk}
# Requires: Visual Studio 2017/2019/2022 with C++ workload; PowerShell 5.1+
# =============================================================================

param(
    [Parameter(Position = 0)]
    [ValidateSet('build','clean','help')]
    [string]$Command = 'help',

    [Parameter(Position = 1)]
    [ValidateSet('release','debug')]
    [string]$Configuration = 'release',

    [Parameter(Position = 2)]
    [ValidateSet('x86','x64')]
    [string]$Architecture = 'x64',

    [switch]$help,

    # Override: explicit path to vcvars*.bat (skips auto-detection)
    [string]$VcVarsPath = ''
)

$ErrorActionPreference = 'Stop'
$Repo         = $PSScriptRoot
$Configuration = $Configuration.ToLower()
$Architecture = $Architecture.ToLower()
$BuildRoot    = Join-Path $Repo 'build'
$ArchDir      = Join-Path $BuildRoot $Architecture
$ConfigDir    = Join-Path $ArchDir $Configuration
$Src          = 'gguf-tools.c gguflib.c sds.c fp16.c'

# ---------------------------------------------------------------------------
# HELP
# ---------------------------------------------------------------------------
if ($Command -eq 'help' -or $help) {
    @"
gguf-tools build script (MSVC)

Usage:
  build-msvc.ps1 build [release|debug] [x86|x64]
  build-msvc.ps1 clean

Commands:
  build    Compile gguf-tools
  clean    Delete build artifacts (keeps directories)

Options:
  -VcVarsPath <path>    Explicit vcvars*.bat path (skips auto-detection)
  -help                 Show this help

Examples:
  .\build-msvc.ps1 build                     -> release x64
  .\build-msvc.ps1 build debug x86           -> debug x86
  .\build-msvc.ps1 clean                     -> remove all outputs
"@
    exit 0
}

# ---------------------------------------------------------------------------
# Locate vcvars*.bat for the requested architecture
# ---------------------------------------------------------------------------
function Find-VcVars {
    param([string]$arch)

    if ($VcVarsPath -and (Test-Path $VcVarsPath)) { return $VcVarsPath }

    $vcvarsName = if ($arch -eq 'x64') { 'vcvars64.bat' } else { 'vcvars32.bat' }

    # Primary: vswhere.exe (ships with every VS 2017+ install)
    $pf86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($installPath) {
            $p = Join-Path $installPath "VC\Auxiliary\Build\$vcvarsName"
            if (Test-Path $p) { return $p }
        }
    }

    # Fallback: common install paths
    $suffixes = @(
        'C:\Program Files\Microsoft Visual Studio\2022',
        'C:\Program Files\Microsoft Visual Studio\2019',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019',
        'C:\Program Files (x86)\Microsoft Visual Studio\2017',
        'D:\Microsoft Visual Studio\2022',
        'D:\Microsoft Visual Studio\2019'
    )
    foreach ($sfx in $suffixes) {
        $pat = "$sfx\*\VC\Auxiliary\Build\$vcvarsName"
        $m = Get-Item $pat -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($m) { return $m.FullName }
    }

    throw "Cannot find $vcvarsName. Install VS with 'Desktop development " +
          "with C++' workload, or pass -VcVarsPath."
}

# ---------------------------------------------------------------------------
# CLEAN — delete build artifacts, keep directory structure
# ---------------------------------------------------------------------------
if ($Command -eq 'clean') {
    $deleted = 0
    foreach ($arch in @('x86','x64')) {
        foreach ($cfg in @('release','debug')) {
            $dir = Join-Path (Join-Path $BuildRoot $arch) $cfg
            if (-not (Test-Path $dir)) { continue }
            $items = @(Get-ChildItem $dir -Force -ErrorAction SilentlyContinue)
            $n = $items.Count
            if ($n -eq 0) { continue }
            Remove-Item $items -Force
            Write-Host "[clean] build\${arch}\${cfg}\  ($n files deleted)"
            $deleted += $n
        }
    }
    if ($deleted -eq 0) { Write-Host "[clean] build/ is already empty" }
    exit 0
}

# ---------------------------------------------------------------------------
# BUILD
# ---------------------------------------------------------------------------
New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
$vcvars = Find-VcVars -arch $Architecture

$flags = switch ($Configuration) {
    'release' { '/nologo /O2 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /utf-8' }
    'debug'   { '/nologo /Od /Zi /D_CRT_SECURE_NO_WARNINGS /utf-8' }
}

Write-Host "vcvars      : $vcvars"
Write-Host "config      : $Configuration"
Write-Host "arch        : $Architecture"
Write-Host "output      : $ConfigDir"
Write-Host ""

# cd /d $ConfigDir : compile from build dir so vc140.pdb lands there too
$srcs    = ($Src -split ' ') | ForEach-Object { Join-Path $Repo $_ }
$exeName = "gguf-tools.exe"
$fo      = "./"
$cmd     = "`"$vcvars`" && cd /d `"$ConfigDir`" && cl $flags -I`"$Repo`" $srcs /Fo:`"$fo`" /Fe:`"$exeName`""

& cmd.exe /c $cmd 2>&1
$exit = $LASTEXITCODE
Write-Host ""

$fe = "$ConfigDir\gguf-tools.exe"
if ($exit -eq 0 -and (Test-Path $fe)) {
    $exe = Get-Item $fe
    Write-Host "[ok] $($exe.FullName)  ($([math]::Round($exe.Length/1MB,3)) MB)"
} else {
    Write-Host "[fail] cl.exe exited with code $exit"
}
exit $exit
