<#
.SYNOPSIS
    Creates (or removes) a SELF-SIGNED code-signing certificate for testing the signing steps.

.DESCRIPTION
    Windows does not trust a self-signed certificate, so a program signed with it is
    still flagged by SmartScreen. It only proves that the signing pipeline works.
    For real releases get a certificate from a certificate authority, or use a free
    signing service for open source (see docs/SIGNING.md).

    The certificate is created in the CurrentUser store. Nothing is added to the
    "Trusted Root" store, so the rest of the system is not affected.

.EXAMPLE
    .\tools\new-dev-cert.ps1            # create, prints the thumbprint
    .\tools\new-dev-cert.ps1 -Remove    # delete every "BadCode Guard (dev)" certificate
#>
param([switch]$Remove)

$subject = 'CN=BadCode Guard (dev)'

if ($Remove) {
    $found = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -like '*BadCode Guard (dev*' }
    $found | ForEach-Object { Remove-Item $_.PSPath }
    Write-Host "Removed $(@($found).Count) certificate(s)."
    return
}

$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject `
    -CertStoreLocation Cert:\CurrentUser\My -KeyAlgorithm RSA -KeyLength 3072 `
    -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(2)
Write-Host "Created: $($cert.Subject)"
Write-Host "Thumbprint: $($cert.Thumbprint)"
Write-Host "Sign with: .\tools\sign.ps1 -Path dist\BadCodeGuard.exe -Thumbprint $($cert.Thumbprint) -AllowUntrusted"
