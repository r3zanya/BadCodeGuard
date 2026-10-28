$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
# $Part is prepended by the host: 'hw' (hardware) or 'os' (Windows). The two
# parts run in parallel processes because several WMI queries are slow.

function Cim($cls, $ns, $filter) {
    $p = @{ ClassName = $cls }
    if ($ns) { $p.Namespace = $ns }
    if ($filter) { $p.Filter = $filter }
    Get-CimInstance @p
}
function Iso($d) { if ($d) { ([datetime]$d).ToString('yyyy-MM-dd') } else { $null } }

$cs = Cim Win32_ComputerSystem

if ($Part -eq 'hw') {
    $bb = Cim Win32_BaseBoard
    $bios = Cim Win32_BIOS

    $cpu = @(Cim Win32_Processor | ForEach-Object {
        [ordered]@{
            name = ($_.Name -replace '\s+', ' ').Trim()
            manufacturer = $_.Manufacturer
            cores = $_.NumberOfCores
            threads = $_.NumberOfLogicalProcessors
            maxMhz = $_.MaxClockSpeed
            l2Kb = $_.L2CacheSize
            l3Kb = $_.L3CacheSize
            socket = $_.SocketDesignation
            architecture = $_.Architecture
            virtualization = $_.VirtualizationFirmwareEnabled
        }
    })

    # Accurate VRAM comes from the driver registry key; WMI caps it at 4 GB.
    $vram = @{}
    Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}' | ForEach-Object {
        $p = Get-ItemProperty $_.PSPath
        $m = $p.'HardwareInformation.qwMemorySize'
        if ($m -is [byte[]]) { $m = [BitConverter]::ToUInt64($m, 0) }
        if ($p.DriverDesc -and $m) { $vram[$p.DriverDesc] = [int64]$m }
    }
    $gpu = @(Cim Win32_VideoController | ForEach-Object {
        $v = $vram[$_.Name]
        if (-not $v) { $v = [int64]$_.AdapterRAM }
        [ordered]@{
            name = $_.Name
            vendor = $_.AdapterCompatibility
            driverVersion = $_.DriverVersion
            driverDate = Iso $_.DriverDate
            vramBytes = $v
            width = $_.CurrentHorizontalResolution
            height = $_.CurrentVerticalResolution
            refresh = $_.CurrentRefreshRate
        }
    })

    $ram = @(Cim Win32_PhysicalMemory | ForEach-Object {
        [ordered]@{
            slot = $_.DeviceLocator
            bytes = [int64]$_.Capacity
            manufacturer = ($_.Manufacturer -replace '\s+', ' ').Trim()
            part = ($_.PartNumber -replace '\s+', ' ').Trim()
            speed = $_.Speed
            configuredSpeed = $_.ConfiguredClockSpeed
            smbiosType = $_.SMBIOSMemoryType
        }
    })

    $disks = @(Get-PhysicalDisk | ForEach-Object {
        [ordered]@{
            name = $_.FriendlyName
            mediaType = [string]$_.MediaType
            busType = [string]$_.BusType
            bytes = [int64]$_.Size
            health = [string]$_.HealthStatus
        }
    })
    $volumes = @(Get-Volume | Where-Object { $_.DriveType -eq 'Fixed' -and $_.DriveLetter } | ForEach-Object {
        [ordered]@{
            letter = [string]$_.DriveLetter
            label = $_.FileSystemLabel
            fs = $_.FileSystem
            bytes = [int64]$_.Size
            freeBytes = [int64]$_.SizeRemaining
        }
    })

    $result = [ordered]@{
        device = [ordered]@{
            manufacturer = $cs.Manufacturer
            model = $cs.Model
            systemType = $cs.SystemType
            sku = $cs.SystemSKUNumber
            isLaptop = (@(Cim Win32_Battery).Count -gt 0)
        }
        board = [ordered]@{ manufacturer = $bb.Manufacturer; product = $bb.Product; version = $bb.Version }
        bios = [ordered]@{ vendor = $bios.Manufacturer; version = $bios.SMBIOSBIOSVersion; date = Iso $bios.ReleaseDate }
        cpu = $cpu
        gpu = $gpu
        ram = $ram
        totalRamBytes = [int64]$cs.TotalPhysicalMemory
        disks = $disks
        volumes = $volumes
        sound = @(Cim Win32_SoundDevice | ForEach-Object { $_.Name })
        network = @(Get-NetAdapter -Physical | Where-Object { $_.Status -eq 'Up' } | ForEach-Object {
            [ordered]@{ name = $_.InterfaceDescription; speed = [string]$_.LinkSpeed }
        })
    }
}
else {
    $os = Cim Win32_OperatingSystem
    $cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'

    $activation = $null
    $lic = Cim SoftwareLicensingProduct $null "ApplicationId='55c92734-d682-4d71-983e-d6ec3f16059f' AND PartialProductKey IS NOT NULL" | Select-Object -First 1
    if ($lic) { $activation = [ordered]@{ status = [int]$lic.LicenseStatus; description = $lic.Description } }

    $tz = Get-TimeZone
    $homeLoc = Get-WinHomeLocation
    $dg = Cim Win32_DeviceGuard 'root\Microsoft\Windows\DeviceGuard' | Select-Object -First 1
    $vmms = Get-Service vmms
    $mp = Get-MpComputerStatus
    $sys = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System'

    $installDate = $null
    if ($cv.InstallDate) { $installDate = ([datetime]'1970-01-01').AddSeconds($cv.InstallDate).ToString('yyyy-MM-dd') }

    $result = [ordered]@{
        windows = [ordered]@{
            caption = ($os.Caption -replace '^Microsoft\s+', '')
            displayVersion = $cv.DisplayVersion
            edition = $cv.EditionID
            build = $cv.CurrentBuild
            ubr = $cv.UBR
            architecture = $os.OSArchitecture
            installDate = $installDate
            lastBoot = if ($os.LastBootUpTime) { $os.LastBootUpTime.ToString('yyyy-MM-dd HH:mm') } else { $null }
            computerName = $env:COMPUTERNAME
            activation = $activation
            systemLocale = (Get-WinSystemLocale).DisplayName
            uiLanguage = (Get-UICulture).DisplayName
            region = $homeLoc.HomeLocation
            timeZone = $tz.DisplayName
            hypervisorPresent = $cs.HypervisorPresent
            vmms = if ($vmms) { [string]$vmms.Status } else { $null }
            vbsStatus = $dg.VirtualizationBasedSecurityStatus
            securityServicesRunning = @($dg.SecurityServicesRunning)
            securityServicesConfigured = @($dg.SecurityServicesConfigured)
            codeIntegrityPolicy = $dg.CodeIntegrityPolicyEnforcementStatus
            userCodeIntegrityPolicy = $dg.UsermodeCodeIntegrityPolicyEnforcementStatus
            defenderEnabled = $mp.AntivirusEnabled
            defenderRealtime = $mp.RealTimeProtectionEnabled
            defenderSignatures = $mp.AntivirusSignatureVersion
            thirdPartyAv = @(Cim AntiVirusProduct 'root\SecurityCenter2' | ForEach-Object { $_.displayName })
            uac = $sys.EnableLUA
        }
    }
}
$result | ConvertTo-Json -Depth 6 -Compress
