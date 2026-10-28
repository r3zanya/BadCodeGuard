<#
.SYNOPSIS
    Signs Windows executables with an Authenticode certificate.

.DESCRIPTION
    Takes the certificate from (in this order):
      -Thumbprint    a certificate installed in the CurrentUser or LocalMachine "My" store
      -PfxFile       a .pfx/.p12 file; its password comes from -PfxPassword or the
                     BCG_PFX_PASSWORD environment variable
      BCG_PFX_FILE   environment variable with the path of a .pfx file
    Signs with SHA-256 and a trusted timestamp, so the signature stays valid after
    the certificate expires. Then verifies the result.

    A certificate that Windows does not trust (for example the self-signed one from
    new-dev-cert.ps1) produces a signature that is intact but shown as "not trusted".
    That is useful for testing the build, not for releases: pass -AllowUntrusted to
    accept it.

.EXAMPLE
    .\tools\sign.ps1 -Path dist\BadCodeGuard.exe -PfxFile C:\keys\codesign.pfx
#>
param(
    [Parameter(Mandatory = $true)][string[]]$Path,
    [string]$Thumbprint,
    [string]$PfxFile = $env:BCG_PFX_FILE,
    [string]$PfxPassword = $env:BCG_PFX_PASSWORD,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [switch]$AllowUntrusted
)

$ErrorActionPreference = 'Stop'

function Get-SigningCertificate {
    if ($Thumbprint) {
        foreach ($store in 'Cert:\CurrentUser\My', 'Cert:\LocalMachine\My') {
            $c = Get-ChildItem $store | Where-Object Thumbprint -eq $Thumbprint | Select-Object -First 1
            if ($c) { return $c }
        }
        throw "No certificate with thumbprint $Thumbprint in the CurrentUser or LocalMachine store."
    }
    if ($PfxFile) {
        if (-not (Test-Path $PfxFile)) { throw "Certificate file not found: $PfxFile" }
        $flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
        return [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($PfxFile, $PfxPassword, $flags)
    }
    throw 'No certificate given. Use -Thumbprint or -PfxFile (or set BCG_PFX_FILE).'
}

$cert = Get-SigningCertificate
if (-not $cert.HasPrivateKey) { throw 'The certificate has no private key.' }
if ($cert.NotAfter -lt (Get-Date)) { throw "The certificate expired on $($cert.NotAfter)." }
$isCodeSigning = $cert.EnhancedKeyUsageList | Where-Object { $_.ObjectId -eq '1.3.6.1.5.5.7.3.3' }
if (-not $isCodeSigning) { throw 'The certificate is not valid for code signing.' }
Write-Host "Signing with: $($cert.Subject)"

$failed = 0
foreach ($file in $Path) {
    if (-not (Test-Path $file)) { Write-Host "  missing: $file"; $failed++; continue }
    $result = Set-AuthenticodeSignature -FilePath $file -Certificate $cert -HashAlgorithm SHA256 `
        -TimestampServer $TimestampUrl -ErrorAction Continue
    $check = Get-AuthenticodeSignature -FilePath $file
    $stamped = [bool]$check.TimeStamperCertificate
    $ok = ($check.Status -eq 'Valid') -or ($AllowUntrusted -and $check.SignerCertificate -and
        $check.Status -in 'UnknownError', 'NotTrusted')
    $state = "{0}, timestamp: {1}" -f $check.Status, $(if ($stamped) { 'yes' } else { 'NO' })
    if ($ok -and $stamped) {
        Write-Host "  signed: $file ($state)"
    } else {
        Write-Host "  FAILED: $file ($state) $($result.StatusMessage)"
        $failed++
    }
}
if ($failed -gt 0) { exit 1 }
