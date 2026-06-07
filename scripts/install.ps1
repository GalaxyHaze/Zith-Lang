param(
    [string]$Version = ""
)

$Repo = "GalaxyHaze/Zith"
$IsMusl = $false
# Simple arg parse: check for --musl in args
if ($args -contains "--musl") { $IsMusl = $true }

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "No version specified. Fetching latest version..."
    try {
        $Response = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest"
        $Version = $Response.tag_name
        Write-Host "Latest version found: $Version" -ForegroundColor Green
    } catch {
        Write-Error "Failed to fetch latest version from GitHub. Check your internet connection."
        exit 1
    }
} else {
    Write-Host "Installing requested version: $Version" -ForegroundColor Yellow
}

# Detect OS and architecture
$FileName = ""
$IsArm64 = $false

# Check if we're on an ARM64 Windows
$Arch = (Get-CimInstance -Class Win32_Processor | Select-Object -First 1).Caption
if ($Arch -match "ARM" -or $env:PROCESSOR_ARCHITECTURE -match "ARM64") {
    $IsArm64 = $true
}

if ($IsMusl) {
    if ($IsArm64) { $FileName = "zithc-windows-arm64-musl.exe" }
    else          { $FileName = "zithc-windows-amd64-musl.exe" }
} elseif ($IsArm64) {
    $FileName = "zithc-windows-arm64.exe"
} else {
    $FileName = "zithc-windows-amd64.exe"
}

$DownloadUrl = "https://github.com/$Repo/releases/download/$Version/$FileName"
$TempPath = "$env:TEMP\zith-installer.exe"
$InstallDir = "$env:LOCALAPPDATA\Microsoft\WindowsApps"

Write-Host "Downloading from $DownloadUrl..." -ForegroundColor Cyan

try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $TempPath -UseBasicParsing
} catch {
    Write-Error "Failed to download file. The version '$Version' might not exist."
    exit 1
}

Write-Host "Installing Zith to $InstallDir..."

try {
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    Copy-Item -Path $TempPath -Destination "$InstallDir\zith.exe" -Force
    Remove-Item -Path $TempPath -Force

    Write-Host "--------------------------------------------------"
    Write-Host "Installation Complete!" -ForegroundColor Green
    Write-Host "Run 'zith --help' in a NEW terminal window to get started."
    Write-Host "--------------------------------------------------"
} catch {
    Write-Error "Failed to install to $InstallDir."
    Write-Host "You might need to run PowerShell as Administrator, or manually move the file from:"
    Write-Host "$TempPath"
    Write-Host "to a folder in your PATH."
    exit 1
}
