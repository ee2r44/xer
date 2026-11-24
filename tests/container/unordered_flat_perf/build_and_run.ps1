# Build+run unordered flat perf (MSVC). Run from repo root or this dir.
$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$Build = Join-Path $Repo "tests\container\unordered_flat_perf\build-msvc"
$Vcvars = "D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat"
$Out = Join-Path $Repo "tests\container\_perf_out_unordered_flat_msvc.txt"
$Exe = Join-Path $Repo "tests\container\xer_unordered_flat_table_perf_msvc.exe"

New-Item -ItemType Directory -Force -Path $Build | Out-Null
cmd /c "call `"$Vcvars`" >nul && cmake -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DPERF_SUFFIX=msvc -S `"$PSScriptRoot`" -B `"$Build`" && cmake --build `"$Build`" --config Release"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Exe | Tee-Object -FilePath $Out
exit $LASTEXITCODE
