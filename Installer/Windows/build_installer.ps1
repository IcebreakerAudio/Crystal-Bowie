# Builds the Release CLAP/VST3 plugins (unless -SkipBuild) and compiles the Inno Setup installer.
#
# Usage:
#   .\build_installer.ps1              # build Release, then compile the installer
#   .\build_installer.ps1 -SkipBuild   # assume Release binaries already exist, just compile
#
# Reusing this for a future sibling project: the build target names (CrystalBowie_clap/CrystalBowie_vst3
# below) and artifact paths are JUCE/Pamplejuce-specific (${PROJECT_NAME}_${FORMAT} target names, a
# build\<Project>_artefacts\<Config>\<Format>\ layout) — a clap-wrapper-based sibling (see MegaBass2020/
# Slope-Overload-CLAP-Version's copies of this script) names both differently. Everything else — reading
# product identity from build\Installer\ProductInfo.iss, invoking ISCC.exe — is the same.

param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$buildDir = Join-Path $repoRoot "build"
$productInfoPath = Join-Path $buildDir "Installer\ProductInfo.iss"

if (-not (Test-Path $productInfoPath)) {
    throw "Missing $productInfoPath. Run 'cmake -B build' at the repo root first, which generates this file from CMakeLists.txt's PRODUCT_NAME/COMPANY_NAME/BUNDLE_ID identity variables."
}

$productInfo = Get-Content $productInfoPath -Raw
function Get-DefineValue([string]$name) {
    $match = [regex]::Match($productInfo, "#define\s+$name\s+`"([^`"]+)`"")
    if (-not $match.Success) { throw "Could not find #define $name in $productInfoPath" }
    return $match.Groups[1].Value
}
$outputName = Get-DefineValue "MyOutputName"
$appName = Get-DefineValue "MyAppName"
$appVersion = Get-DefineValue "MyAppVersion"

if (-not $SkipBuild) {
    Write-Host "Building Release CLAP/VST3 targets..."
    cmake --build $buildDir --config Release --target CrystalBowie_VST3 CrystalBowie_CLAP
    if ($LASTEXITCODE -ne 0) { throw "cmake --build failed with exit code $LASTEXITCODE" }
}

$clapPath = Join-Path $buildDir "CrystalBowie_artefacts\Release\CLAP\$outputName.clap"
$vst3Path = Join-Path $buildDir "CrystalBowie_artefacts\Release\VST3\$outputName.vst3"
foreach ($path in @($clapPath, $vst3Path)) {
    if (-not (Test-Path $path)) {
        throw "Expected build artifact not found: $path`nBuild the Release configuration first (see CLAUDE.md's Build section), or drop -SkipBuild to build it now."
    }
}

$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($isccCommand) {
    $isccPath = $isccCommand.Source
} else {
    $defaultPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (Test-Path $defaultPath) {
        $isccPath = $defaultPath
    } else {
        throw "ISCC.exe not found on PATH or at '$defaultPath'. Install Inno Setup 6 (https://jrsoftware.org/isinfo.php)."
    }
}

Write-Host "Compiling installer with $isccPath..."
& $isccPath (Join-Path $scriptDir "CrystalBowie.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC.exe failed with exit code $LASTEXITCODE" }

Write-Host "Done: Installer\Windows\Output\$appName-$appVersion-Setup.exe"
