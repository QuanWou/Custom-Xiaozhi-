$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compilerDir = 'C:\msys64\ucrt64\bin'
if (-not (Test-Path "$compilerDir/g++.exe")) { throw 'Install MinGW UCRT64 gcc/g++ first.' }
$env:PATH = $compilerDir + ';' + $env:PATH
$motionTestDir = Join-Path ([System.IO.Path]::GetTempPath()) ('droid-motion-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $motionTestDir | Out-Null
Push-Location $taskRoot
try {
    & "$compilerDir/gcc.exe" -c managed_components/espressif__cjson/cJSON/cJSON.c -o "$motionTestDir/cjson.o"
    if ($LASTEXITCODE -ne 0) { throw 'cJSON host compilation failed.' }
    & "$compilerDir/g++.exe" -std=c++17 -Wall -Wextra -Werror -static -I main -I main/boards/custom-s3-voice -I managed_components/espressif__cjson/cJSON scripts/tests/droid_motion_program_test.cc main/boards/custom-s3-voice/droid_motion_program.cc main/robot_face_ui/robot_face_animator.cc "$motionTestDir/cjson.o" -o "$motionTestDir/motion-tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Motion host compilation failed.' }
    & "$motionTestDir/motion-tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Motion host tests failed.' }
} finally { Pop-Location }
Write-Output "Test artifacts: $motionTestDir"
