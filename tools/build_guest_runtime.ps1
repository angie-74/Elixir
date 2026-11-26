# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
param(
  [string]$BuildDir = '',
  [string]$Platform = 'android-31'
)
. "$PSScriptRoot\elixir_env.ps1"
if (-not $BuildDir) { $BuildDir = Join-Path $ElixirEnv.BuildRoot 'guest-runtime' }

$source = Join-Path $PSScriptRoot '..\guest'
& $ElixirEnv.CMake -S $source -B $BuildDir -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$($ElixirEnv.Ninja)" `
  "-DCMAKE_TOOLCHAIN_FILE=$($ElixirEnv.NdkToolchain)" `
  -DANDROID_ABI=x86_64 "-DANDROID_PLATFORM=$Platform" `
  -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'guest runtime configure failed' }
& $ElixirEnv.CMake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw 'guest runtime build failed' }
Get-ChildItem $BuildDir -Filter *.so | Select-Object Name, Length
