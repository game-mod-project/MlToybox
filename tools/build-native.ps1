param([ValidateSet('Release', 'Debug')][string]$Config = 'Release', [switch]$Test)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmake = Join-Path $cmakeDir 'CMake\bin\cmake.exe'
$ctest = Join-Path $cmakeDir 'CMake\bin\ctest.exe'
$ninja = Join-Path $cmakeDir 'Ninja\ninja.exe'
$src = Join-Path $repo 'native'
$build = Join-Path $src 'build'
$cmd = "`"$vcvars`" >nul && `"$cmake`" -S `"$src`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=$Config && `"$cmake`" --build `"$build`""
if ($Test) { $cmd += " && `"$ctest`" --test-dir `"$build`" --output-on-failure" }
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "native build failed ($LASTEXITCODE)" }
Write-Host "Built $build\mltoybox_native.dll"
