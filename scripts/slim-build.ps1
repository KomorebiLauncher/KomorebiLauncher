# SPDX-License-Identifier: GPL-3.0-only
#
# Keeps a local build directory from growing past what a build actually needs.
#
# Debug links here run with /LTCG, so every target writes a ~100 MB PDB, and a map file on top of that
# when the build was configured for it -- fifty tests at once is several gigabytes that no test reads.
# The launcher's own PDB is kept because a crash is the one case where it is needed; everything else is
# regenerated the next time that target is linked.
#
# Usage: powershell -File scripts/slim-build.ps1 [-BuildDir build-local] [-KeepLauncher]

param(
    [string]$BuildDir = "build-local",
    [switch]$KeepLauncher
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BuildDir)) {
    Write-Host "no build directory at $BuildDir"
    exit 0
}

$before = (Get-ChildItem $BuildDir -Recurse -File -Force | Measure-Object Length -Sum).Sum
$removed = 0
$count = 0

# Map and incremental-link files are pure debugging leftovers; nothing reads them unless a link is being
# inspected by hand.
Get-ChildItem $BuildDir -Recurse -File -Force -Include *.map, *.ilk -ErrorAction SilentlyContinue | ForEach-Object {
    $removed += $_.Length
    $count++
    Remove-Item $_.FullName -Force
}

# Keep the launcher's symbols when asked; delete every other program database.
Get-ChildItem $BuildDir -Recurse -File -Force -Include *.pdb -ErrorAction SilentlyContinue | Where-Object {
    -not ($KeepLauncher -and $_.Name -eq "komorebilauncher.pdb")
} | ForEach-Object {
    $removed += $_.Length
    $count++
    Remove-Item $_.FullName -Force
}

# The temporary folder the build script redirects to.
$tmp = Join-Path $BuildDir "tmp"
if (Test-Path $tmp) {
    Get-ChildItem $tmp -Recurse -File -Force -ErrorAction SilentlyContinue | ForEach-Object {
        $removed += $_.Length
        $count++
        Remove-Item $_.FullName -Force
    }
}

$after = (Get-ChildItem $BuildDir -Recurse -File -Force | Measure-Object Length -Sum).Sum
Write-Host ("removed {0} files, {1:N2} GB; {2} now {3:N2} GB" -f $count, ($removed / 1GB), $BuildDir, ($after / 1GB))
