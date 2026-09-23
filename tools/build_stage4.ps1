param(
    [string]$VersionA = "6.0.0",
    [string]$VersionB = "7.0.0",
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
    foreach ($variant in @("FaultInjection", "Release")) {
        $fault = if ($variant -eq "FaultInjection") { "1" } else { "0" }
        Invoke-Checked $make @("-s", "-B", "-C", "Bootloader", "all", "pbl-flexnvm", "FAULT_INJECTION=$fault")
        $destination = Join-Path $project "Application_Build/Stage4/$variant"
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        foreach ($extension in @("elf", "srec", "map")) {
            Copy-Item -LiteralPath "Bootloader/build/Bootloader.$extension" -Destination $destination -Force
        }
    }
    foreach ($slot in @("A", "B")) {
        $version = if ($slot -eq "A") { $VersionA } else { $VersionB }
        $directory = "Application_Build/Stage4/APP_$slot"
        Invoke-Checked $make @("-s", "-B", "-f", "Makefile.application_v2", "all", "SLOT=$slot", "BUILD_DIR=$directory")
        Invoke-Checked (Join-Path $NodeBin "node.exe") @("tools/pack_image.js", "--slot", $slot,
            "--input", "$directory/APP_$slot.bin", "--output", "$directory/app_slot_$($slot.ToLower())_image.bin", "--version", $version)
    }
    Invoke-Checked (Join-Path $NodeBin "node.exe") @("tools/tests/stage2_host_tests.js")
    Invoke-Checked $Python @("tools/check_stage3_layout.py", "--objdump", (Join-Path $ArmBin "arm-none-eabi-objdump.exe"),
                           "--app-root", "Application_Build/Stage4")
    $env:PATH = "$NativeBin;$MsysBin;$previousPath"
    $common = @("-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror", "-IBootloader/inc", "-Iinclude")
    $sources = @("Bootloader/src/boot_programming.c", "Bootloader/src/boot_slot.c", "Bootloader/src/boot_version.c",
        "Bootloader/src/boot_journal.c", "Bootloader/src/boot_update.c", "Bootloader/src/boot_recovery_policy.c",
        "Bootloader/src/boot_image_install.c", "tools/tests/stage5_compat_stubs.c")
    Invoke-Checked (Join-Path $NativeBin "gcc.exe") ($common + @("tools/tests/stage3_programming_test.c") + $sources +
        @("Bootloader/src/boot_uds.c", "-o", "tools/tests/stage3_programming_test.exe"))
    Invoke-Checked (Join-Path $project "tools/tests/stage3_programming_test.exe") @()
    Invoke-Checked (Join-Path $NativeBin "gcc.exe") ($common + @("-DBOOT_ENABLE_FAULT_INJECTION=1", "tools/tests/stage4_journal_test.c") +
        $sources + @("-o", "tools/tests/stage4_journal_test.exe"))
    Invoke-Checked (Join-Path $project "tools/tests/stage4_journal_test.exe") @()
    Invoke-Checked (Join-Path $NativeBin "gcc.exe") ($common + @("tools/tests/stage4_isotp_test.c", "Bootloader/src/boot_isotp.c",
        "-o", "tools/tests/stage4_isotp_test.exe"))
    Invoke-Checked (Join-Path $project "tools/tests/stage4_isotp_test.exe") @()
    $env:PYTHONPATH = Join-Path $project "linux_gateway"
    Invoke-Checked $Python @("-m", "unittest", "discover", "-s", "linux_gateway/tests", "-v")
    Write-Output "PASS Stage4 SOFTWARE build/regression/recovery tests. Physical Stage4 power-loss tests PENDING."
    Write-Output "Artifacts: Application_Build/Stage4; original APP_A/APP_B stage3 artifacts preserved."
} finally {
    $env:PATH = $previousPath
    $env:PYTHONPATH = $previousPythonPath
    Pop-Location
}
