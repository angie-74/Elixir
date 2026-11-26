# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
param(
  [ValidateSet('Debug', 'Release', 'RelWithDebInfo')][string]$Config = 'RelWithDebInfo',
  [string]$BuildDir = '',
  [switch]$Test,
  [switch]$Guest,
  [switch]$Lto,
  [string[]]$Target = @(),
  [string[]]$Define = @()
)
. "$PSScriptRoot\elixir_env.ps1"
Enter-ElixirMsvcEnvironment
$Target = @($Target | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Define = @($Define | ForEach-Object { $_ -split ',' } | Where-Object { $_ } | ForEach-Object { "-D$_" })
if (-not $BuildDir) { $BuildDir = Join-Path $ElixirEnv.BuildRoot 'windows' }
$guestDir = Join-Path $ElixirEnv.BuildRoot 'guest'
$guestRuntimeDir = Join-Path $ElixirEnv.BuildRoot 'guest-runtime'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = '*'
if ($Guest) {
  $guestTests = Join-Path $PSScriptRoot 'build_guest_tests.ps1'
  if (Test-Path $guestTests) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $guestTests -BuildDir $guestDir
    if ($LASTEXITCODE -ne 0) { throw 'guest test build failed' }
  }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build_guest_runtime.ps1') -BuildDir $guestRuntimeDir
  if ($LASTEXITCODE -ne 0) { throw 'guest runtime build failed' }
}

$clang = $ElixirEnv.ClangCl -replace '\\', '/'
$linker = (Get-Command link.exe).Source -replace '\\', '/'
$ltoValue = if ($Lto) { "ON" } else { "OFF" }
$llvmTools = @()
if ($Lto) {
  $toolDir = Join-Path $BuildDir 'llvm-tools'
  New-Item -ItemType Directory -Force -Path $toolDir | Out-Null
  $lld = Join-Path $ElixirEnv.NdkBin 'ld.lld.exe'
  $lldLink = Join-Path $toolDir 'lld-link.exe'
  if (-not (Test-Path $lldLink) -or (Get-Item $lldLink).LastWriteTimeUtc -ne (Get-Item $lld).LastWriteTimeUtc) {
    Copy-Item $lld $lldLink -Force
  }
  $linker = $lldLink -replace '\\', '/'
  $llvmTools = @("-DCMAKE_AR=$((Join-Path $ElixirEnv.NdkBin 'llvm-lib.exe') -replace '\\', '/')")
  $env:PATH = "$($ElixirEnv.NdkBin);$env:PATH"
}
& $ElixirEnv.CMake -S $root -B $BuildDir -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$($ElixirEnv.Ninja)" `
  "-DCMAKE_BUILD_TYPE=$Config" `
  "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clang" `
  "-DCMAKE_LINKER=$linker" `
  "-DELIXIR_ENABLE_LTO=$ltoValue" @llvmTools `
  "-DELIXIR_GUEST_TEST_DIR=$($guestDir -replace '\\', '/')" `
  "-DELIXIR_GUEST_RUNTIME_DIR=$($guestRuntimeDir -replace '\\', '/')" `
  @Define
if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
if ($Target.Count -gt 0) {
  & $ElixirEnv.CMake --build $BuildDir --target @Target
} else {
  & $ElixirEnv.CMake --build $BuildDir
}
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
if ($Test) {
  if ($Target.Count -gt 0) {
    foreach ($name in $Target) {
      $exe = Get-ChildItem -Path $BuildDir -Recurse -Filter "$name.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
      if ($exe) {
        & $exe.FullName
        if ($LASTEXITCODE -ne 0) { throw "$name failed" }
      }
    }
  } else {
    & $ElixirEnv.CTest --test-dir $BuildDir --output-on-failure -j 8
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
  }
}
