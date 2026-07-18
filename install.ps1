[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA "Toy"),
    [switch]$AddToPath
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    throw "InstallDir must not be empty"
}

$SourceRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$ToyExecutable = Join-Path $SourceRoot "bin\toy.exe"
if (-not (Test-Path -LiteralPath $ToyExecutable -PathType Leaf)) {
    throw "This installer must be run from a staged Toy SDK (missing $ToyExecutable)"
}

$DestinationRoot = [System.IO.Path]::GetFullPath($InstallDir)
$SourceWithSeparator = $SourceRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($DestinationRoot.StartsWith(
        $SourceWithSeparator,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to install a Toy SDK inside itself: $DestinationRoot"
}

if (-not $DestinationRoot.Equals(
        $SourceRoot,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
    Get-ChildItem -LiteralPath $SourceRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $DestinationRoot `
            -Recurse -Force
    }
}

$InstalledBin = Join-Path $DestinationRoot "bin"
if (-not (Test-Path -LiteralPath (Join-Path $InstalledBin "toy.exe") -PathType Leaf)) {
    throw "Toy installation did not produce $InstalledBin\toy.exe"
}

if ($AddToPath) {
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $Entries = @($UserPath -split ';' | Where-Object { $_ -ne "" })
    $AlreadyPresent = $Entries | Where-Object {
        $_.TrimEnd('\') -ieq $InstalledBin.TrimEnd('\')
    }
    if (-not $AlreadyPresent) {
        $UpdatedPath = (@($Entries) + $InstalledBin) -join ';'
        [Environment]::SetEnvironmentVariable("Path", $UpdatedPath, "User")
        $env:Path = "$InstalledBin;$env:Path"
        Write-Host "Added $InstalledBin to the user PATH. Open a new terminal to use it."
    }
}

Write-Host "Installed Toy SDK to $DestinationRoot"
if (-not $AddToPath) {
    Write-Host "Add $InstalledBin to PATH, or rerun with -AddToPath."
}
Write-Host "Run 'toy --help' to verify the installation."
