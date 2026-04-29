<#
.SYNOPSIS
    Downloads Apple II ROM images from the AppleWin project for use with Casso65Emu.

.DESCRIPTION
    Downloads ROM files from AppleWin's GitHub repository and places them in
    the Casso65Emu/roms/ directory.  The roms/ directory is gitignored — ROM
    images are copyrighted by Apple and not distributed with this project.

    Files downloaded:
      - Apple II ROM         (12 KB) → apple2.rom
      - Apple II+ ROM        (12 KB) → apple2plus.rom
      - Apple IIe ROM        (16 KB) → apple2e.rom
      - Apple II Video ROM   (2 KB)  → apple2-video.rom   (character generator)
      - Apple IIe Video ROM  (4 KB)  → apple2e-video.rom  (character generator + MouseText)
      - Disk II Boot ROM     (256 B) → disk2.rom           (slot 6 boot firmware)

    Source: https://github.com/AppleWin/AppleWin/tree/master/resource

.PARAMETER Force
    If set, re-downloads files even if they already exist.

.NOTES
    Exit codes: 0 = success, 1 = failure
#>
[CmdletBinding()]
param (
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir
$romsDir   = Join-Path $repoRoot 'roms'

$baseUrl = 'https://raw.githubusercontent.com/AppleWin/AppleWin/master/resource'

# Map: AppleWin filename → our filename
$romFiles = @(
    @{ Source = 'Apple2.rom';                  Dest = 'apple2.rom';        Size = 12288;  Desc = 'Apple II ROM (Integer BASIC)' },
    @{ Source = 'Apple2_Plus.rom';             Dest = 'apple2plus.rom';    Size = 12288;  Desc = 'Apple II+ ROM (Applesoft BASIC)' },
    @{ Source = 'Apple2e.rom';                 Dest = 'apple2e.rom';       Size = 16384;  Desc = 'Apple IIe ROM' },
    @{ Source = 'Apple2_Video.rom';            Dest = 'apple2-video.rom';  Size = 2048;   Desc = 'Apple II/II+ Character Generator ROM' },
    @{ Source = 'Apple2e_Enhanced_Video.rom';  Dest = 'apple2e-video.rom'; Size = 4096;   Desc = 'Apple IIe Character Generator ROM (MouseText)' },
    @{ Source = 'DISK2.rom';                   Dest = 'disk2.rom';         Size = 256;    Desc = 'Disk II Boot ROM (slot 6)' }
)

# Create roms directory
if (-not (Test-Path $romsDir)) {
    New-Item -ItemType Directory -Path $romsDir -Force | Out-Null
    Write-Host "Created: $romsDir"
}

$downloaded = 0
$skipped    = 0
$failed     = 0

foreach ($rom in $romFiles) {
    $destPath = Join-Path $romsDir $rom.Dest
    $url      = "$baseUrl/$($rom.Source)"

    if ((Test-Path $destPath) -and -not $Force) {
        $fileSize = (Get-Item $destPath).Length

        if ($fileSize -eq $rom.Size) {
            Write-Host "  SKIP  $($rom.Dest) ($($rom.Desc)) — already exists" -ForegroundColor DarkGray
            $skipped++
            continue
        }

        Write-Host "  SIZE  $($rom.Dest) — wrong size ($fileSize, expected $($rom.Size)), re-downloading" -ForegroundColor Yellow
    }

    Write-Host "  GET   $($rom.Dest) ($($rom.Desc))..." -NoNewline

    try {
        Invoke-WebRequest -Uri $url -OutFile $destPath -UseBasicParsing

        $fileSize = (Get-Item $destPath).Length

        if ($fileSize -ne $rom.Size) {
            Write-Host " SIZE MISMATCH ($fileSize bytes, expected $($rom.Size))" -ForegroundColor Red
            Remove-Item $destPath -Force
            $failed++
        }
        else {
            Write-Host " OK ($fileSize bytes)" -ForegroundColor Green
            $downloaded++
        }
    }
    catch {
        Write-Host " FAILED: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Downloaded: $downloaded  Skipped: $skipped  Failed: $failed"

if ($failed -gt 0) {
    Write-Host "Some ROM downloads failed. The emulator may not work without all ROM files." -ForegroundColor Red
    exit 1
}

Write-Host "ROM files are in: $romsDir" -ForegroundColor Green
Write-Host "NOTE: ROM images are Apple copyrighted material sourced from AppleWin." -ForegroundColor DarkYellow
