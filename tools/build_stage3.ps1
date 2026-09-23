param(
    [string]$ImageVersion = "3.0.0",
    [string]$ArmBin = "C:\NXP\S32DS.3.6.8\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin",
    [string]$MsysBin = "C:\NXP\S32DS.3.6.8\S32DS\build_tools\msys32\usr\bin",
    [string]$NativeBin = "C:\NXP\S32DS_ARM_v2.2\S32DS\build_tools\msys32\mingw32\bin",
    [string]$Python = "C:\Users\27695\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe",
    [string]$NodeBin = "C:\Users\27695\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin"
)
$ErrorActionPreference = "Stop"
$project = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$previousPath = $env:PATH
$previousPythonPath = $env:PYTHONPATH
function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed: $LASTEXITCODE" }
}
Push-Location $project
try {
    $env:PATH = "$ArmBin;$MsysBin;$NodeBin;$previousPath"
    $make = Join-Path $MsysBin "make.exe"
    Invoke-Checked $make @("-s", "-B", "-C", "Bootloader", "all", "pbl-flexnvm")
    Invoke-Checked $make @("-s", "-B", "-f", "Makefile.application_v2", "images", "IMAGE_VERSION=$ImageVersion")
    Invoke-Checked (Join-Path $NodeBin "node.exe") @("tools/tests/stage2_host_tests.js")
    Invoke-Checked $Python @("tools/check_stage3_layout.py", "--objdump", (Join-Path $ArmBin "arm-none-eabi-objdump.exe"))
    $env:PATH = "$NativeBin;$MsysBin;$previousPath"
    Invoke-Checked (Join-Path $NativeBin "gcc.exe") @(
        "-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
        "-IBootloader/inc", "-Iinclude", "tools/tests/stage3_programming_test.c",
        "Bootloader/src/boot_programming.c", "Bootloader/src/boot_slot.c",
        "Bootloader/src/boot_version.c", "Bootloader/src/boot_uds.c",
        "Bootloader/src/boot_journal.c", "Bootloader/src/boot_update.c",
        "Bootloader/src/boot_recovery_policy.c", "Bootloader/src/boot_image_install.c",
        "tools/tests/stage5_compat_stubs.c",
        "-o", "tools/tests/stage3_programming_test.exe")
    Invoke-Checked (Join-Path $project "tools/tests/stage3_programming_test.exe") @()
    $env:PYTHONPATH = Join-Path $project "linux_gateway"
    Invoke-Checked $Python @("-m", "unittest", "discover", "-s", "linux_gateway/tests", "-v")
    Write-Output "PASS Stage3 regression software build/tests with Stage4 Journal. Stage3 board baseline was user-accepted; Stage4 board tests pending."
} finally {
    $env:PATH = $previousPath
    $env:PYTHONPATH = $previousPythonPath
    Pop-Location
}
