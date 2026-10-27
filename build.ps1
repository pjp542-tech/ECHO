[CmdletBinding()]
param(
    [ValidateSet('Build', 'Clean', 'Flash', 'Doctor')]
    [string]$Action = 'Build',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$bundleRoots = @(
    $env:STM32CUBE_BUNDLE_ROOT
    $env:CUBE_BUNDLE_PATH
    if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'stm32cube/bundles' }
    if ($env:USERPROFILE) { Join-Path $env:USERPROFILE 'AppData/Local/stm32cube/bundles' }
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique

function Find-BuildTool([string]$Name, [string]$Bundle, [switch]$Optional) {
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) { return $command.Source }

    foreach ($root in $bundleRoots) {
        $package = Join-Path $root $Bundle
        if (-not (Test-Path -LiteralPath $package)) { continue }
        $versions = Get-ChildItem -LiteralPath $package -Directory |
            Sort-Object LastWriteTimeUtc -Descending
        foreach ($version in $versions) {
            $candidate = Join-Path $version.FullName "bin/$Name.exe"
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }

    # Also recognise the standalone Windows CubeProgrammer installation.
    if ($Name -eq 'STM32_Programmer_CLI' -and $env:ProgramFiles) {
        $candidate = Join-Path $env:ProgramFiles 'STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    if (-not $Optional) {
        throw "Missing $Name. Install the STM32Cube tools listed in README.md, put the tool on PATH, or set STM32CUBE_BUNDLE_ROOT to your bundles folder."
    }
}

function Invoke-BuildTool([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed (exit code $LASTEXITCODE)." }
}

$savedPath = $env:PATH
Push-Location $PSScriptRoot
try {
    $cmake = Find-BuildTool 'cmake' 'cmake'
    $ninja = Find-BuildTool 'ninja' 'ninja'
    $compiler = Find-BuildTool 'arm-none-eabi-gcc' 'gnu-tools-for-stm32'
    $toolDirs = @($compiler, $cmake, $ninja) | ForEach-Object { Split-Path -Parent $_ }
    $env:PATH = ($toolDirs -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $savedPath

    if ($Action -eq 'Doctor') {
        Write-Host "CMake:    $cmake"
        Write-Host "Ninja:    $ninja"
        Write-Host "Compiler: $compiler"
        Invoke-BuildTool $cmake @('--version')
        Invoke-BuildTool $ninja @('--version')
        Invoke-BuildTool $compiler @('--version')
        $programmer = Find-BuildTool 'STM32_Programmer_CLI' 'programmer' -Optional
        if ($programmer) { Write-Host "Flasher:  $programmer" }
        else { Write-Host 'Flasher: not installed (only needed to program the board).' }
    }
    else {
        Invoke-BuildTool $cmake @(
            "-DECHO_PROJECT_DIR=$PSScriptRoot", "-DECHO_CONFIGURATION=$Configuration",
            '-P', (Join-Path $PSScriptRoot 'cmake/reset-moved-cache.cmake')
        )
        Invoke-BuildTool $cmake @('--preset', $Configuration)
        if ($Action -eq 'Clean') {
            Invoke-BuildTool $cmake @('--build', '--preset', $Configuration, '--target', 'clean')
        }
        else {
            Invoke-BuildTool $cmake @('--build', '--preset', $Configuration, '--parallel')
            $firmware = Join-Path $PSScriptRoot "build/$Configuration/ECHO.elf"
            Write-Host "Firmware: $firmware"
            if ($Action -eq 'Flash') {
                $programmer = Find-BuildTool 'STM32_Programmer_CLI' 'programmer'
                Invoke-BuildTool $programmer @('-c', 'port=SWD', '-w', $firmware, '-v', '-rst')
            }
        }
    }
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
finally {
    $env:PATH = $savedPath
    Pop-Location
}
