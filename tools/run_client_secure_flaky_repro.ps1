param(
    [string]$BuildDir = "build/client-secure",
    [int]$Iterations = 3
)

$ErrorActionPreference = "Stop"

if ($Iterations -lt 1) {
    throw "Iterations must be >= 1"
}

$tests = @(
    "device_sync_ratchet_test",
    "sdk_c_api_e2e_test"
)

Write-Host "BuildDir=$BuildDir"
Write-Host "Iterations=$Iterations"

for ($i = 1; $i -le $Iterations; $i++) {
    Write-Host "=== Iteration $i/$Iterations ==="
    foreach ($test in $tests) {
        Write-Host "--- Running $test ---"
        ctest -C Release --output-on-failure -R "^$test$" --test-dir $BuildDir
    }
}

Write-Host "All requested iterations finished."
