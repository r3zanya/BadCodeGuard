# BadCode Guard

An open-source antivirus for Windows 10/11 written in C++ and Qt 6. It scans files against
hash databases, YARA rules and heuristics, watches folders and newly started programs,
quarantines threats and keeps a journal.

*Русский: [README.md](README.md)*

> **Note.** This is an educational/experimental project, not a replacement for Microsoft
> Defender. It has no kernel driver, does not block execution before the check, and relies on
> small public databases. Use it alongside Windows' own protection, not instead of it.

## Features

- Quick, full and folder scans with an accurate progress bar and time estimate.
- SHA-256 hash lists (free MalwareBazaar feed, your own lists, extra URLs), YARA rules (libyara),
  heuristics for executables, scripts, shortcuts, PDF and Office documents.
- Archives: zip, jar, Office files in memory; 7z, rar, tar, gz, cab, iso and more via Windows' `tar.exe`.
- Real-time protection of Downloads, Desktop, Temp (risky file types only), removable drives and
  custom folders; process start control that terminates and quarantines known threats.
- Quarantine with restore, exclusions, scheduled scans, findings journal with CSV export.
- Optional VirusTotal hash lookup (your own API key; only the hash is sent, never the file).
- API keys stored with Windows DPAPI; optional protection of the process against termination.

## Install

Download `BadCodeGuard-<version>-win64.zip` from **Releases**, verify it against `SHA256SUMS.txt`,
unzip and run `BadCodeGuard.exe`. If the executable is not signed, SmartScreen will warn: see
[docs/SIGNING.md](docs/SIGNING.md).

The bundled database contains only the EICAR test hash. For real detection get a free MalwareBazaar
key from [abuse.ch](https://abuse.ch/) and use **Settings → Database update**.

## Build

Requires Visual Studio (C++ desktop workload), Qt 6.8+ for MSVC 2022 64-bit and CMake.

```
build.bat                 :: set QT_DIR if Qt is not under C:\Qt
cmake --build build --target bcg_tests && build\bcg_tests.exe
```

## Privacy

No telemetry. Network requests: list updates (abuse.ch and URLs you provide), the YARA rule set
(github.com) and, only if you enter a key, VirusTotal (hash only). See [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE). Third-party components: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
