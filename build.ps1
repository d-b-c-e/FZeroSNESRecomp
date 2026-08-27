$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'build'
cmake -S $Root -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $Build --config Release --parallel
exit $LASTEXITCODE
