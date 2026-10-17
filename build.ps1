<#
    Build the plugin, after holding the engine to its checks.

        .\build.ps1                  engine check, then the VST3
        .\build.ps1 -NoPlugin        engine check only, for iterating on the DSP
        .\build.ps1 -Wav <dir>       and write demo phrases to <dir> to listen to
        .\build.ps1 -Release         and the files a release attaches, into dist\: the installer
                                     and a zip of the VST3

    The check gates the plugin rather than standing in for it: if the engine aliases, runs away, is off
    the grid or blows up, there is no point wrapping a plugin round it, and if it passes the
    plugin is built from those same sources straight after.

    The patterns and the clock are MzPCF's, compiled from its repository beside this one
    (..\MzPCF), as JUCE is from ..\JUCE.

    cl.exe is never on the PATH - vcvars64.bat sets up INCLUDE, LIB and PATH for it, and
    vswhere says where Visual Studio is rather than a versioned path being written down here.
#>
param(
    [switch]$NoPlugin,
    [switch]$Release,
    [string]$Wav = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "No Visual Studio found. Install VS Community with 'Desktop development with C++'."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                     -property installationPath
if (-not $vsPath) {
    Write-Error "Visual Studio is installed but the C++ tools are not. Add 'Desktop development with C++'."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

# ---------------------------------------------------------------- the engine check

$out = Join-Path $env:TEMP "mzpcdcheck"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$mzpcf = Join-Path (Split-Path -Parent $root) "MzPCF"
if (-not (Test-Path (Join-Path $mzpcf "Source\PCF\Pcf.cpp"))) {
    Write-Error "No MzPCF at $mzpcf - clone https://github.com/simozzer/MzPCF beside this repository."
}

$exe = Join-Path $out "PcdCheck.exe"
$sources = @(
    (Join-Path $root "Tests\PcdCheck.cpp"),
    (Join-Path $root "Source\PCD\Pcd.cpp"),
    (Join-Path $mzpcf "Source\PCF\Pcf.cpp")
)
$includes = @((Join-Path $root "Source\PCD"), (Join-Path $mzpcf "Source")) | ForEach-Object { "/I `"$_`"" }
$quoted = ($sources | ForEach-Object { "`"$_`"" }) -join " "

Write-Host "building the engine check"
cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$out`" && cl /nologo /std:c++17 /EHsc /W4 /O2 /fp:fast $($includes -join ' ') $quoted /Fe:`"$exe`"" |
    Where-Object { $_ -match "error|warning" }
if ($LASTEXITCODE -ne 0) { Write-Error "the engine check did not build" }

if ($Wav -ne "") {
    New-Item -ItemType Directory -Force -Path $Wav | Out-Null
    & $exe --wav (Resolve-Path $Wav)
} else {
    & $exe
}
if ($LASTEXITCODE -ne 0) { Write-Error "the engine failed its checks - not building a plugin around it" }

if ($NoPlugin) { exit 0 }

# ---------------------------------------------------------------- the plugin

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $bundled) { $cmake = $bundled }
}
if (-not $cmake) { Write-Error "No cmake on the PATH and none inside Visual Studio." }

$buildDir = Join-Path $root "build"

if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    Write-Host ""
    Write-Host "configuring $buildDir (first time)"
    & $cmake -S $root -B $buildDir
    if ($LASTEXITCODE -ne 0) { Write-Error "cmake configure failed" }
}

Write-Host ""
Write-Host "building the VST3"

# The version and copyright are written down once, in CMakeLists.txt, and the binaries and
# installer are stamped from them. JUCE writes the Windows version resource once and never
# again, so when CMakeLists.txt has changed since, the old one is thrown away here, or the
# binaries go on saying the old version and copyright.
$cmakeListsPath = Join-Path $root "CMakeLists.txt"
$cmakeLists = Get-Content $cmakeListsPath -Raw
if ($cmakeLists -notmatch "project\(MzPCD VERSION ([0-9.]+)") { Write-Error "no version in CMakeLists.txt" }
$version = $Matches[1]

$rc = Join-Path $buildDir "MzPCD_artefacts\JuceLibraryCode\MzPCD_resources.rc"
if ((Test-Path $rc) -and (Get-Item $rc).LastWriteTime -lt (Get-Item $cmakeListsPath).LastWriteTime) {
    Remove-Item $rc
}

& $cmake --build $buildDir --config Release --target MzPCD_VST3 -- /m /v:minimal |
    Where-Object { $_ -match "error|warning C|installed to|could not be" }
if ($LASTEXITCODE -ne 0) { Write-Error "the plugin build failed" }

# Report what is on disk, and when it was written, rather than that the build returned zero.
$artefact = Join-Path $buildDir "MzPCD_artefacts\Release\VST3\MzPCD.vst3\Contents\x86_64-win\MzPCD.vst3"

$newest = Get-ChildItem (Join-Path $root "Source") -Recurse -Include *.cpp, *.h |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1

Write-Host ""
if (-not (Test-Path $artefact)) { Write-Error "expected $artefact and it is not there" }
$f = Get-Item $artefact
$stale = $f.LastWriteTime -lt $newest.LastWriteTime
Write-Host ("ok  -  {0,-12} {1,7} KB   {2}{3}" -f $f.Name, [math]::Round($f.Length / 1KB), $f.LastWriteTime,
            $(if ($stale) { "   *** OLDER THAN $($newest.Name) ***" } else { "" })) `
           -ForegroundColor $(if ($stale) { "Red" } else { "Green" })
if ($stale) { Write-Error "the plugin is older than the sources it was built from" }

if (-not $Release) { exit 0 }

# ---------------------------------------------------------------- the installer

$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
            Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { Write-Error "No Inno Setup 6. Install it from https://jrsoftware.org/isdl.php" }

Write-Host ""
Write-Host "building the installer for $version"

& $iscc /Q "/DAppVersion=$version" (Join-Path $root "installer\MzPCD.iss")
if ($LASTEXITCODE -ne 0) { Write-Error "the installer build failed" }

# ---------------------------------------------------------------- the zip, for installing by hand

Write-Host "building the zip"

$dist = Join-Path $root "dist"
$stage = Join-Path $buildDir "zip\VST3"
$readme = (Get-Content (Join-Path $root "installer\README.txt") -Raw).Replace("@VERSION@", $version)

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item (Join-Path $buildDir "MzPCD_artefacts\Release\VST3\MzPCD.vst3") $stage -Recurse
Copy-Item (Join-Path $root "LICENSE") $stage
Set-Content (Join-Path $stage "README.txt") $readme -NoNewline

$zip = Join-Path $dist "MzPCD-$version-win64-VST3.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

# Report what is on disk, as for the build.
Write-Host ""
foreach ($kind in "Setup.exe", "VST3.zip") {
    $f = Get-Item (Join-Path $dist "MzPCD-$version-win64-$kind")
    Write-Host ("ok  -  {0,-34} {1,5:N1} MB   {2}" -f $f.Name, ($f.Length / 1MB), $f.LastWriteTime) -ForegroundColor Green
}

exit 0
