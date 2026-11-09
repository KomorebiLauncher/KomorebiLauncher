# SPDX-License-Identifier: GPL-3.0-only
#
# Makes Ninja's dependency tracking work when MSVC speaks a language other than English.
#
# Ninja parses the compiler's `/showIncludes` notes with a literal prefix that CMake writes into
# CMakeFiles/rules.ninja. CMake obtains that prefix by reading the compiler's output during configure,
# and on a machine whose console and compiler messages are not in English the text it writes back can
# end up differently encoded from what the compiler prints later. When it does, Ninja matches nothing,
# records no dependencies, and a header change rebuilds only the file that changed -- the stale objects
# that follow are the source of crashes that make no sense.
#
# The fix is to ask the compiler what it actually prints now (as bytes, through the same pipe Ninja
# uses) and write that exact text back as UTF-8. It does not assume a language or a codepage.
#
# Usage: powershell -File scripts/fix-ninja-msvc-prefix.ps1 [-BuildDir build-local]

param(
    [string]$BuildDir = "build-local"
)

$ErrorActionPreference = "Stop"

$rules = Join-Path $BuildDir "CMakeFiles/rules.ninja"
if (-not (Test-Path $rules)) {
    Write-Host "no rules.ninja under $BuildDir; is this a Ninja build directory?"
    exit 0
}

# Compile one file that includes something, capturing the compiler's notes exactly as Ninja would see
# them (through a pipe, so the compiler picks the same encoding).
$probe = Join-Path ([System.IO.Path]::GetTempPath()) "komorebi-showincludes-probe.cpp"
[System.IO.File]::WriteAllText($probe, "#include <cstdio>`nint main() { return 0; }`n", (New-Object System.Text.ASCIIEncoding))
try {
    $output = & cl.exe /nologo /showIncludes /c $probe /Fo"NUL" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "the compiler probe failed; dependency prefix left as it was"
        exit 1
    }
}
finally {
    Remove-Item $probe -Force -ErrorAction SilentlyContinue
}

# The prefix is everything before the include's path, e.g. "Note: including file:" or "注意: 包含文件:".
$prefix = $null
foreach ($line in $output) {
    $text = [string]$line
    if ($text -match '^(.*?):\s+[A-Za-z]:[\\/]') {
        $prefix = $Matches[1] + ":"
        break
    }
}
if (-not $prefix) {
    Write-Host "could not recognise the compiler's include lines; dependency prefix left as it was"
    exit 1
}

# Rewrite just that one line, byte for byte, so nothing else in the file is re-encoded.
$bytes = [System.IO.File]::ReadAllBytes($rules)
$marker = [System.Text.Encoding]::ASCII.GetBytes("msvc_deps_prefix = ")
$start = -1
for ($index = 0; $index -le $bytes.Length - $marker.Length; $index++) {
    $same = $true
    for ($offset = 0; $offset -lt $marker.Length; $offset++) {
        if ($bytes[$index + $offset] -ne $marker[$offset]) {
            $same = $false
            break
        }
    }
    if ($same) {
        $start = $index
        break
    }
}
if ($start -lt 0) {
    Write-Host "rules.ninja has no msvc_deps_prefix line; nothing to fix"
    exit 0
}

$end = $start
while ($end -lt $bytes.Length -and $bytes[$end] -ne 10) {
    $end++
}

$replacement = [System.Text.Encoding]::UTF8.GetBytes("msvc_deps_prefix = " + $prefix)
$rewritten = New-Object System.Collections.Generic.List[byte]
for ($index = 0; $index -lt $start; $index++) { $rewritten.Add($bytes[$index]) }
$rewritten.AddRange($replacement)
for ($index = $end; $index -lt $bytes.Length; $index++) { $rewritten.Add($bytes[$index]) }
[System.IO.File]::WriteAllBytes($rules, $rewritten.ToArray())

# Ninja keeps its recorded dependencies in a separate log; the entries made before this fix hold no
# files at all, so they have to go or the old objects look up to date forever.
Remove-Item (Join-Path $BuildDir ".ninja_deps") -Force -ErrorAction SilentlyContinue

Write-Host ("dependency prefix set to '{0}' as UTF-8" -f $prefix)
