param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

function Resolve-EmsdkRoot {
    $candidates = @()

    if ($env:EMSDK) {
        $candidates += $env:EMSDK
    }

    $candidates += @(
        (Join-Path $repoRoot "emsdk"),
        (Join-Path (Split-Path $repoRoot -Parent) "emsdk")
    )

    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }

        $toolchain = Join-Path $candidate "upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake"
        if (Test-Path $toolchain) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw @"
EMSDK was not found.

Install emsdk and either:
  1. set the EMSDK environment variable, or
  2. place the emsdk folder next to this repo.

Then rerun .\build.ps1
"@
}

function Prepend-Path([string]$PathEntry) {
    if (-not $PathEntry -or -not (Test-Path $PathEntry)) {
        return
    }

    $currentEntries = $env:PATH -split ";"
    if ($currentEntries -contains $PathEntry) {
        return
    }

    $env:PATH = "$PathEntry;$env:PATH"
}

$emsdkRoot = Resolve-EmsdkRoot
$env:EMSDK = $emsdkRoot

$emsdkEnvScript = Join-Path $emsdkRoot "emsdk_env.ps1"
if (Test-Path $emsdkEnvScript) {
    . $emsdkEnvScript | Out-Null
} else {
    $emscriptenDir = Join-Path $emsdkRoot "upstream\emscripten"
    Prepend-Path $emsdkRoot
    Prepend-Path $emscriptenDir

    $nodeDir = Get-ChildItem (Join-Path $emsdkRoot "node") -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($nodeDir) {
        $nodeBin = Join-Path $nodeDir.FullName "bin"
        Prepend-Path $nodeBin

        $nodeExe = Join-Path $nodeBin "node.exe"
        if (Test-Path $nodeExe) {
            $env:EMSDK_NODE = $nodeExe
        }
    }

    $pythonDir = Get-ChildItem (Join-Path $emsdkRoot "python") -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($pythonDir) {
        Prepend-Path $pythonDir.FullName

        $pythonExe = Join-Path $pythonDir.FullName "python.exe"
        if (Test-Path $pythonExe) {
            $env:EMSDK_PYTHON = $pythonExe
        }
    }
}

$preset = if ($Configuration -eq "Release") { "wasm-release" } else { "wasm-debug" }

Push-Location $repoRoot
try {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    cmake --build --preset $preset
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
