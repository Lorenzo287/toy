[CmdletBinding()]
param(
    [string]$Toy = (Join-Path $PSScriptRoot "..\build\clang\release\toy.exe"),
    [ValidateRange(1, 1000)]
    [int]$Runs = 5,
    [string[]]$Benchmark
)

$ErrorActionPreference = "Stop"

$toyPath = (Resolve-Path -LiteralPath $Toy).Path
if ($Benchmark) {
    $scripts = foreach ($name in $Benchmark) {
        $filename = if ($name.EndsWith(".toy")) { $name } else { "$name.toy" }
        Get-Item -LiteralPath (Join-Path $PSScriptRoot $filename)
    }
} else {
    $scripts = Get-ChildItem -LiteralPath $PSScriptRoot -Filter "*.toy" |
        Sort-Object Name
}

foreach ($script in $scripts) {
    Write-Host "`n$($script.Name)"
    $samples = [System.Collections.Generic.List[double]]::new()

    for ($run = 1; $run -le $Runs; $run++) {
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        & $toyPath $script.FullName
        $exitCode = $LASTEXITCODE
        $timer.Stop()

        if ($exitCode -ne 0) {
            throw "$($script.Name) failed with exit code $exitCode"
        }

        $samples.Add($timer.Elapsed.TotalMilliseconds)
        Write-Host ("run {0}: {1:N3} ms wall" -f $run,
                    $timer.Elapsed.TotalMilliseconds)
    }

    $sorted = @($samples | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    $median = if ($sorted.Count % 2 -eq 1) {
        $sorted[$middle]
    } else {
        ($sorted[$middle - 1] + $sorted[$middle]) / 2
    }
    Write-Host ("median: {0:N3} ms wall" -f $median)
}
