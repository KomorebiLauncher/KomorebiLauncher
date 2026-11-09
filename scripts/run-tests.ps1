# SPDX-License-Identifier: GPL-3.0-only
#
# Runs the test suite with everything it writes kept inside the build directory.
#
# ctest's own temporary directories, and whatever a test makes with QTemporaryDir, follow TMP/TEMP; by
# default those point at the system drive, which is where a run of fifty tests leaves its leftovers. The
# plugin path and offscreen platform are needed because the tests load Qt from the toolchain that built
# them rather than from a system-wide install.
#
# Usage: powershell -File scripts/run-tests.ps1 [-BuildDir build-local] [-Config Debug]

param(
    [string]$BuildDir = "build-local",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$buildRoot = Resolve-Path $BuildDir
$tmp = Join-Path $buildRoot "tmp"
New-Item -ItemType Directory -Force $tmp | Out-Null
$env:TMP = $tmp
$env:TEMP = $tmp

$qt = Join-Path $env:LOCALAPPDATA "KomorebiLauncherLite\Qt\6.10.3\msvc2022_64"
if (Test-Path $qt) {
    $env:PATH = (Join-Path $qt "bin") + ";" + $env:PATH
    $env:QT_PLUGIN_PATH = Join-Path $qt "plugins"
}
$env:QT_QPA_PLATFORM = "offscreen"

ctest --test-dir $BuildDir -C $Config
exit $LASTEXITCODE
