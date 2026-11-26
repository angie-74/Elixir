# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

$ErrorActionPreference = 'Stop'

function Find-ElixirVisualStudio {
  if ($env:ELIXIR_VS -and (Test-Path $env:ELIXIR_VS)) { return $env:ELIXIR_VS }
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found: install Visual Studio 2022 Build Tools with the C++ workload.' }
  $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $path) { throw 'No Visual Studio installation with the MSVC x64 toolset was found.' }
  return $path
}

function Find-ElixirNdk {
  $candidates = @()
  foreach ($name in 'ELIXIR_NDK', 'ANDROID_NDK_HOME', 'ANDROID_NDK_ROOT', 'ANDROID_NDK') {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { $candidates += $value }
  }
  $sdkRoots = @()
  foreach ($name in 'ANDROID_HOME', 'ANDROID_SDK_ROOT') {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { $sdkRoots += $value }
  }
  $sdkRoots += (Join-Path $env:LOCALAPPDATA 'Android\Sdk')
  $packages = Join-Path $env:LOCALAPPDATA 'Packages'
  if (Test-Path $packages) {
    Get-ChildItem $packages -Directory -ErrorAction SilentlyContinue | ForEach-Object {
      $sdk = Join-Path $_.FullName 'LocalCache\Local\Android\Sdk'
      if (Test-Path $sdk) { $sdkRoots += $sdk }
    }
  }
  foreach ($root in $sdkRoots) {
    $ndkDir = Join-Path $root 'ndk'
    if (Test-Path $ndkDir) {
      Get-ChildItem $ndkDir -Directory | Sort-Object { [version]($_.Name -replace '[^0-9.].*$', '') } -Descending |
        ForEach-Object { $candidates += $_.FullName }
    }
  }
  foreach ($candidate in $candidates) {
    $toolchain = Join-Path $candidate 'build\cmake\android.toolchain.cmake'
    $clang = Join-Path $candidate 'toolchains\llvm\prebuilt\windows-x86_64\bin\clang-cl.exe'
    if ((Test-Path $toolchain) -and (Test-Path $clang)) { return (Resolve-Path $candidate).Path }
  }
  throw 'Android NDK not found. Install "ndk;30.0.16248370" with sdkmanager or set ELIXIR_NDK.'
}

$vs = Find-ElixirVisualStudio
$ndk = Find-ElixirNdk
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$ElixirEnv = [ordered]@{
  VisualStudio = $vs
  VcVars       = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
  CMake        = Join-Path $cmakeDir 'CMake\bin\cmake.exe'
  CTest        = Join-Path $cmakeDir 'CMake\bin\ctest.exe'
  Ninja        = Join-Path $cmakeDir 'Ninja\ninja.exe'
  Ndk          = $ndk
  NdkBin       = Join-Path $ndk 'toolchains\llvm\prebuilt\windows-x86_64\bin'
  NdkToolchain = Join-Path $ndk 'build\cmake\android.toolchain.cmake'
}
foreach ($key in 'VcVars', 'CMake', 'Ninja') {
  if (-not (Test-Path $ElixirEnv[$key])) { throw "$key not found at $($ElixirEnv[$key])" }
}
$ElixirEnv.ClangCl = Join-Path $ElixirEnv.NdkBin 'clang-cl.exe'

function Get-ElixirBuildRoot {
  if ($env:ELIXIR_BUILD_ROOT) { return $env:ELIXIR_BUILD_ROOT }
  $repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
  $isLocal = $false
  try {
    $qualifier = Split-Path -Qualifier $repo
    $drive = [System.IO.DriveInfo]::new($qualifier)
    $isLocal = $drive.DriveType -eq [System.IO.DriveType]::Fixed
  } catch { $isLocal = $false }
  if ($repo.StartsWith('\')) { $isLocal = $false }
  if ($isLocal) { return (Join-Path $repo 'build') }
  return (Join-Path $env:USERPROFILE 'ElixirBuild')
}
$ElixirEnv.BuildRoot = Get-ElixirBuildRoot

function Enter-ElixirMsvcEnvironment {
  if ($env:ELIXIR_MSVC_ENV_LOADED -eq '1') { return }
  $dump = & cmd.exe /c "`"$($ElixirEnv.VcVars)`" >nul 2>&1 && set"
  if ($LASTEXITCODE -ne 0) { throw 'vcvars64.bat failed' }
  foreach ($line in $dump) {
    $eq = $line.IndexOf('=')
    if ($eq -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0, $eq), $line.Substring($eq + 1)) }
  }
  $env:ELIXIR_MSVC_ENV_LOADED = '1'
}
