# UI smoke test: starts the program, opens every page through the sidebar, takes a
# picture of the window and checks that each page was really drawn and that the
# program is still alive at the end. Exit code 0 = all good.
#
#   powershell -File tests\ui_smoke.ps1 -Exe D:\BadCodeGuard\dist\BadCodeGuard.exe [-OutDir C:\temp\shots]

param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [string]$OutDir = (Join-Path $env:TEMP 'bcg_ui_smoke')
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class BcgWin {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@

New-Item -ItemType Directory -Force $OutDir | Out-Null
$failed = 0
function Check($ok, $what) {
    if ($ok) { Write-Host "  ok    $what" } else { Write-Host "  FAIL  $what"; $script:failed++ }
}

$proc = Start-Process $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep 4
$proc.Refresh()
$h = $proc.MainWindowHandle
Check ($h -ne [IntPtr]::Zero) 'the main window appears'
if ($h -eq [IntPtr]::Zero) { Stop-Process $proc -ErrorAction SilentlyContinue; exit 1 }
[BcgWin]::MoveWindow($h, 40, 20, 1100, 1400, $true) | Out-Null
Start-Sleep 1

function Snap($name) {
    $r = New-Object BcgWin+RECT
    [BcgWin]::GetWindowRect($h, [ref]$r) | Out-Null
    $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [BcgWin]::PrintWindow($h, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc)
    $path = Join-Path $OutDir "$name.png"
    $bmp.Save($path)
    # Share of lit pixels in the content area (right of the sidebar).
    $lit = 0; $total = 0
    for ($x = 260; $x -lt $bmp.Width - 20; $x += 6) {
        for ($y = 60; $y -lt $bmp.Height - 20; $y += 6) {
            $c = $bmp.GetPixel($x, $y); $total++
            if (($c.R + $c.G + $c.B) -gt 60) { $lit++ }
        }
    }
    $bmp.Dispose()
    return [pscustomobject]@{ Path = $path; Lit = $lit / [math]::Max(1, $total); Hash = (Get-FileHash $path).Hash }
}

# Sidebar entries (client coordinates of the 1st..7th button).
$pages = @(
    @{ Name = 'dashboard';  Y = 124 }, @{ Name = 'scan';       Y = 172 }, @{ Name = 'quarantine'; Y = 220 },
    @{ Name = 'history';    Y = 268 }, @{ Name = 'exclusions'; Y = 316 }, @{ Name = 'system';     Y = 364 },
    @{ Name = 'settings';   Y = 412 }
)
$hashes = @{}
foreach ($p in $pages) {
    $lp = [IntPtr](($p.Y -shl 16) -bor 110)
    [BcgWin]::PostMessage($h, 0x0200, [IntPtr]0, $lp) | Out-Null
    [BcgWin]::PostMessage($h, 0x0201, [IntPtr]1, $lp) | Out-Null
    [BcgWin]::PostMessage($h, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 1500
    $s = Snap $p.Name
    Check ($s.Lit -gt 0.004) ("page '{0}' is drawn ({1:P1} of the content is lit)" -f $p.Name, $s.Lit)
    $hashes[$p.Name] = $s.Hash
}
Check (($hashes.Values | Select-Object -Unique).Count -eq $pages.Count) 'every page looks different from the others'

$proc.Refresh()
Check (-not $proc.HasExited) 'the program is still running after visiting every page'
Stop-Process $proc -ErrorAction SilentlyContinue
Write-Host "pictures: $OutDir"
if ($failed -gt 0) { Write-Host "$failed check(s) failed"; exit 1 }
Write-Host 'all UI checks passed'
exit 0
