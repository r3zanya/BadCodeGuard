<#
.SYNOPSIS
    Builds BadCode Guard and produces the release zip with its SHA-256 checksum.

.DESCRIPTION
    1. builds (build.bat) unless -SkipBuild,
    2. gathers the program, Qt libraries, licences and documentation in release\BadCodeGuard-<version>-win64,
    3. signs the executable when -Sign is given (certificate options as in sign.ps1),
    4. creates the zip and SHA256SUMS.txt next to it.

.EXAMPLE
    .\tools\package.ps1
    .\tools\package.ps1 -Sign -PfxFile C:\keys\codesign.pfx
#>
param(
    [string]$OutDir = 'release',
    [string]$BuildDir = 'build',
    [switch]$SkipBuild,
    [switch]$Sign,
    [string]$Thumbprint,
    [string]$PfxFile = $env:BCG_PFX_FILE,
    [string]$PfxPassword = $env:BCG_PFX_PASSWORD,
    [switch]$AllowUntrusted
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$cmake = Get-Content CMakeLists.txt -Raw
if ($cmake -notmatch 'project\(BadCodeGuard\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Could not read the version from CMakeLists.txt' }
$version = $Matches[1]
$name = "BadCodeGuard-$version-win64"
$stage = Join-Path $OutDir $name
Write-Host "Packaging $name"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

if (-not $SkipBuild) {
    $env:BUILD_DIR = $BuildDir
    cmd /c "`"$(Join-Path $root 'build.bat')`" `"$stage`""
    if ($LASTEXITCODE -ne 0) { throw 'The build failed.' }
} else {
    Copy-Item (Join-Path $BuildDir 'BadCodeGuard.exe') $stage
    Write-Host 'Skipped the build: only the executable was copied, run windeployqt yourself.'
}

# Licences and documentation travel with the binaries (Qt is LGPL: its licence text must come along).
foreach ($f in 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md', 'CHANGELOG.md') {
    if (Test-Path $f) { Copy-Item $f $stage }
}
if (Test-Path 'licenses') { Copy-Item 'licenses' (Join-Path $stage 'licenses') -Recurse }
Copy-Item 'third_party\yara\COPYING' (Join-Path $stage 'licenses\YARA-BSD-3-Clause.txt') -ErrorAction SilentlyContinue
Copy-Item 'third_party\miniz\LICENSE' (Join-Path $stage 'licenses\miniz-MIT.txt') -ErrorAction SilentlyContinue

if ($Sign) {
    $signArgs = @{ Path = (Join-Path $stage 'BadCodeGuard.exe') }
    if ($Thumbprint) { $signArgs.Thumbprint = $Thumbprint }
    if ($PfxFile) { $signArgs.PfxFile = $PfxFile }
    if ($PfxPassword) { $signArgs.PfxPassword = $PfxPassword }
    if ($AllowUntrusted) { $signArgs.AllowUntrusted = $true }
    & (Join-Path $PSScriptRoot 'sign.ps1') @signArgs
    if ($LASTEXITCODE -ne 0) { throw 'Signing failed.' }
}

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $name.zip" | Set-Content (Join-Path $OutDir 'SHA256SUMS.txt') -Encoding ascii

Write-Host ''
Write-Host "Zip:     $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
Write-Host "SHA-256: $hash"
