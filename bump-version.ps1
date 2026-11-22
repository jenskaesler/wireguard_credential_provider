#Requires -Version 5.1
<#
.SYNOPSIS
    Bumps the version of WireGuard Credential Provider (format: YYYY.MM.N).

.DESCRIPTION
    Reads the current version from the DLL resource file, computes the next
    version, and writes it to all version references in the project.

    Version format:  YYYY.MM.N
      YYYY  year
      MM    month, no leading zero
      N     build index within the month, starts at 0, resets on new month

    Files updated:
      resources/WireGuardCredentialProvider.rc
      tray-app/resources/WireGuardTray.rc
      README.md  (version badge)

    Afterwards the three files are committed ("chore: bump version to X"),
    tagged vX and both commit and tag are pushed to origin.

.PARAMETER Patch
    Increment the build index (N+1), keep year and month.

.PARAMETER Month
    New month: use today's year/month, reset N to 0.

.PARAMETER Version
    Set an explicit version string "YYYY.MM.N".

.OUTPUTS
    Exit 0  – no change (cancelled or already up to date)
    Exit 1  – error
    Exit 10 – version was bumped; rebuild the solution in Visual Studio before
              running build.bat again
#>
[CmdletBinding()]
param(
    [switch]$Patch,
    [switch]$Month,
    [string]$Version
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root     = Split-Path $MyInvocation.MyCommand.Path -Parent
$DllRc    = Join-Path $Root 'resources\WireGuardCredentialProvider.rc'
$TrayRc   = Join-Path $Root 'tray-app\resources\WireGuardTray.rc'
$ReadmeMd = Join-Path $Root 'README.md'

# --- Read current version ---
$rcText = [System.IO.File]::ReadAllText($DllRc)
if ($rcText -notmatch 'FILEVERSION\s+(\d+),(\d+),(\d+),(\d+)') {
    Write-Error "Cannot read FILEVERSION from $DllRc"
    exit 1
}
$curYear  = [int]$Matches[1]
$curMonth = [int]$Matches[2]
$curPatch = [int]$Matches[3]
$curVer   = "$curYear.$curMonth.$curPatch"

# --- Compute new version ---
$today    = Get-Date
$newYear  = $today.Year
$newMonth = $today.Month
$newPatch = 0

if ($Version) {
    if ($Version -notmatch '^\d{4}\.\d{1,2}\.\d+$') {
        Write-Error "Version must be YYYY.MM.N  (e.g. 2026.11.0)"
        exit 1
    }
    $p = $Version -split '\.'
    $newYear = [int]$p[0]; $newMonth = [int]$p[1]; $newPatch = [int]$p[2]
}
elseif ($Patch) {
    $newYear = $curYear; $newMonth = $curMonth; $newPatch = $curPatch + 1
}
elseif ($Month) {
    # defaults already point to today
}
else {
    # Interactive menu
    Write-Host ""
    Write-Host "  Current version : $curVer"
    Write-Host ""
    Write-Host "  [1] Patch   ->  $curYear.$curMonth.$($curPatch + 1)"
    Write-Host "  [2] Month   ->  $newYear.$newMonth.0  ($(Get-Date -Format 'MMMM yyyy'))"
    Write-Host "  [3] Manual"
    Write-Host "  [Q] Cancel"
    Write-Host ""
    $choice = Read-Host "  Select"
    switch ($choice.Trim().ToUpper()) {
        '1' { $newYear = $curYear; $newMonth = $curMonth; $newPatch = $curPatch + 1 }
        '2' { }  # defaults already set
        '3' {
            $raw = Read-Host "  Enter version (YYYY.MM.N)"
            if ($raw -notmatch '^\d{4}\.\d{1,2}\.\d+$') {
                Write-Error "Invalid format. Expected YYYY.MM.N"
                exit 1
            }
            $p = $raw -split '\.'
            $newYear = [int]$p[0]; $newMonth = [int]$p[1]; $newPatch = [int]$p[2]
        }
        default { Write-Host "  Cancelled."; exit 0 }
    }
}

$newVer     = "$newYear.$newMonth.$newPatch"
$newVerFull = "$newYear.$newMonth.$newPatch.0"   # 4-part for DLL RC
$newVerRc   = "$newYear,$newMonth,$newPatch,0"   # comma-separated for FILEVERSION

if ($newVer -eq $curVer) {
    Write-Host "  Already at version $curVer. Nothing to do."
    exit 0
}

Write-Host ""
Write-Host "  $curVer  -->  $newVer"
Write-Host ""

# Only ask for confirmation in interactive mode (no flags passed)
$interactive = -not ($Version -or $Patch -or $Month)
if ($interactive) {
    $confirm = Read-Host "  Apply? (y/N)"
    if ($confirm.Trim().ToUpper() -ne 'Y') { Write-Host "  Cancelled."; exit 0 }
}

# --- Git pre-check: abort before touching files if the tag already exists ---
$tag = "v$newVer"
function Invoke-Git { & git -C $Root @args; if ($LASTEXITCODE -ne 0) { Write-Error "git $args failed (exit $LASTEXITCODE)"; exit 1 } }
& git -C $Root rev-parse -q --verify "refs/tags/$tag" *> $null
if ($LASTEXITCODE -eq 0) { Write-Error "Tag $tag already exists."; exit 1 }
# Uncommitted changes would be missing from the tagged release
if (& git -C $Root status --porcelain --untracked-files=no) { Write-Error "Uncommitted changes in the working tree. Commit them first."; exit 1 }

# --- Helper: read file, replace pattern, write back (preserves BOM) ---
function Update-FileRegex($path, $pattern, $replacement) {
    $bytes  = [System.IO.File]::ReadAllBytes($path)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $enc    = [System.Text.UTF8Encoding]::new($hasBom)
    $text   = $enc.GetString($bytes)
    $result = [System.Text.RegularExpressions.Regex]::Replace($text, $pattern, $replacement)
    [System.IO.File]::WriteAllBytes($path, $enc.GetBytes($result))
}

# --- Update DLL RC (4-part version strings: 2026.9.9.0) ---
Update-FileRegex $DllRc 'FILEVERSION(\s+)[\d,]+'         "FILEVERSION`${1}$newVerRc"
Update-FileRegex $DllRc 'PRODUCTVERSION(\s+)[\d,]+'      "PRODUCTVERSION`${1}$newVerRc"
Update-FileRegex $DllRc '(VALUE "FileVersion",\s+")[\d.]+(")'    "`${1}$newVerFull`${2}"
Update-FileRegex $DllRc '(VALUE "ProductVersion",\s+")[\d.]+(")'  "`${1}$newVerFull`${2}"
Write-Host "  [OK] resources\WireGuardCredentialProvider.rc"

# --- Update Tray RC (3-part version strings: 2026.9.9) ---
Update-FileRegex $TrayRc 'FILEVERSION(\s+)[\d,]+'        "FILEVERSION`${1}$newVerRc"
Update-FileRegex $TrayRc 'PRODUCTVERSION(\s+)[\d,]+'     "PRODUCTVERSION`${1}$newVerRc"
Update-FileRegex $TrayRc '(VALUE "FileVersion",\s+")[\d.]+(")'    "`${1}$newVer`${2}"
Update-FileRegex $TrayRc '(VALUE "ProductVersion",\s+")[\d.]+(")'  "`${1}$newVer`${2}"
Write-Host "  [OK] tray-app\resources\WireGuardTray.rc"

# --- Update README version badge ---
Update-FileRegex $ReadmeMd '(Version-)[\d.]+(-blue)' "`${1}$newVer`${2}"
Write-Host "  [OK] README.md"

# --- Commit, tag and push (tag must sit on the bump commit) ---
Invoke-Git commit -q -m "chore: bump version to $newVer" -- resources/WireGuardCredentialProvider.rc tray-app/resources/WireGuardTray.rc README.md
Invoke-Git tag $tag
Invoke-Git push -q
Invoke-Git push -q origin $tag
Write-Host "  [OK] committed, tagged $tag and pushed"

Write-Host ""
Write-Host "  Version bumped: $curVer -> $newVer"
Write-Host ""
Write-Host "  Next steps:"
Write-Host "    1. Rebuild the solution in Visual Studio (Release | x64)"
Write-Host "    2. Run build.bat again to create the installer"
Write-Host "    3. Create the GitHub release for $tag and upload the installer"
Write-Host ""
exit 10
