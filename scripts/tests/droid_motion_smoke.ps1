param([string]$BaseUri = 'http://192.168.1.44:8080')
$ErrorActionPreference = 'Stop'
function Status { Invoke-RestMethod "$BaseUri/api/status" -TimeoutSec 5 }
function Command($Body) {
    Invoke-RestMethod "$BaseUri/api/robot" -Method Post -ContentType 'application/json' -Body ($Body | ConvertTo-Json -Compress -Depth 8) -TimeoutSec 5
}
function Check($Condition, $Message) { if (-not $Condition) { throw $Message } }
function Rejected($Body) {
    $response = Invoke-WebRequest "$BaseUri/api/robot" -Method Post -ContentType 'application/json' -Body ($Body | ConvertTo-Json -Compress -Depth 8) -TimeoutSec 5 -SkipHttpErrorCheck
    Check ($response.StatusCode -eq 400) 'Unsafe/invalid command was not rejected'
    Check (-not ($response.Content | ConvertFrom-Json).success) 'Rejected response must be success=false'
}
$before = Status
Check ($before.firmwareRevision -eq 'droid-custom-motion-r4') 'Expected custom-motion-r4 firmware'
Check ($before.gesture -eq 'idle' -and $before.tracks.left -eq 0 -and $before.tracks.right -eq 0) 'Robot is busy; smoke test aborted without changing it'
foreach ($joint in @('head','leftArm','rightArm')) {
    Check ($before.joints.$joint.angle -eq $before.joints.$joint.target) 'Joint is moving; abort'
}
$valid = @{command='sequence';dry_run=$true;program='{"repeat":5,"steps":[{"pose":{"leftArm":-35,"rightArm":35},"relative":true,"hold_ms":180},{"pose":{"leftArm":0,"rightArm":0},"relative":true,"hold_ms":180}]}'}
Check (Command $valid).success 'Valid simultaneous-arm dry-run failed'
Check ((Status).gesture -eq 'idle') 'Dry-run must not start a motion'
Rejected @{command='sequence';program='{"steps":[{"pose":{"head":181}}]}'}
Rejected @{command='sequence';program='{"steps":[{"drive":"left","duration_ms":999999}]}'}
Rejected @{command='wave_arms';arms='both';cycles=0;amplitude=35}
if ($before.spinCalibration.leftMs -eq 0) { Rejected @{command='spin_turns';direction='left';turns=10} }

# Only wait frames are executed. No joint target or nonzero track speed is requested.
Check (Command @{command='sequence';program='{"repeat":3,"steps":[{"hold_ms":180}]}'}).success 'Wait-only program failed'
$limit = [DateTime]::UtcNow.AddSeconds(5)
do {
    Command @{command='heartbeat'} | Out-Null
    $state = Status
    Check ($state.tracks.left -eq 0 -and $state.tracks.right -eq 0) 'Unexpected track movement'
    if ($state.program.status -eq 'completed') { break }
    Start-Sleep -Milliseconds 150
} while ([DateTime]::UtcNow -lt $limit)
Check ($state.program.status -eq 'completed' -and $state.program.cyclesDone -eq 3) 'Wait repeat count failed'

Check (Command @{command='sequence';program='{"steps":[{"hold_ms":3000}]}'}).success 'Second wait program failed'
Rejected @{command='sequence';program='{"steps":[{"hold_ms":1}]}'}
Check (Command @{command='stop'}).success 'STOP failed'
Check ((Status).program.status -eq 'stopped') 'STOP did not cancel wait program'

Check (Command @{command='sequence';program='{"steps":[{"hold_ms":4000}]}'}).success 'Timeout test program failed'
# No heartbeat: firmware must cancel web program after 1200 ms.
Start-Sleep -Milliseconds 1500
$after = Status
Check ($after.program.status -eq 'heartbeat_timeout') 'Heartbeat failsafe did not trigger (close other controller tabs before testing)'
foreach ($joint in @('head','leftArm','rightArm')) {
    Check ($after.joints.$joint.angle -eq $before.joints.$joint.angle) 'Joint angle changed during wait-only test'
}
Check ($after.tracks.leftSign -eq $before.tracks.leftSign -and $after.tracks.rightSign -eq $before.tracks.rightSign) 'Track calibration changed'
Check ($after.displayRotation -eq $before.displayRotation) 'Display rotation changed'
Write-Output 'PASS: live dry-run, invalid inputs, uncalibrated-spin rejection, wait-only repeats, busy rejection, STOP, heartbeat timeout; no motion requested.'
$after | Select-Object firmwareRevision,program,tracks,spinCalibration,displayRotation,audio | ConvertTo-Json -Depth 5
