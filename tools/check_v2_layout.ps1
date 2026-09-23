param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Objdump = "arm-none-eabi-objdump"
)

$ErrorActionPreference = "Stop"

function Get-ElfSections {
    param([string]$ElfPath)

    $sections = @{}
    & $Objdump -h $ElfPath | ForEach-Object {
        if ($_ -match '^\s*\d+\s+(\S+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+') {
            $sections[$Matches[1]] = @{
                Size = [Convert]::ToUInt32($Matches[2], 16)
                Vma  = [Convert]::ToUInt32($Matches[3], 16)
                Lma  = [Convert]::ToUInt32($Matches[4], 16)
            }
        }
    }
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $ElfPath"
    }
    return $sections
}

$images = @(
    @{ Label='Bootloader'; Path='Bootloader\build\Bootloader.elf'; Vector=0x00000000L; Start=0x00000000L; End=0x00008000L },
    @{ Label='PBL_FlexNVM'; Path='Bootloader\build\PBL_FlexNVM.elf'; Vector=0x10000000L; Start=0x10000000L; End=0x1000C000L },
    @{ Label='APP_A'; Path='Application_Build\APP_A\APP_A.elf'; Vector=0x00008000L; Start=0x00008000L; End=0x0003F000L },
    @{ Label='APP_B'; Path='Application_Build\APP_B\APP_B.elf'; Vector=0x00040000L; Start=0x00040000L; End=0x00077000L }
)

$failures = [System.Collections.Generic.List[string]]::new()
$flashSections = @(
    '.interrupts', '.flash_config', '.text', '.ARM.extab', '.ARM',
    '.init_array', '.fini_array', '.data', '.code'
)
$ramSections = @('.interrupts_ram', '.data', '.code', '.bss', '.heap', '.stack')
foreach ($image in $images) {
    $elf = Join-Path $ProjectRoot $image.Path
    if (-not (Test-Path -LiteralPath $elf)) {
        $failures.Add("$($image.Label): missing $elf")
        continue
    }
    $sections = Get-ElfSections -ElfPath $elf
    if (-not $sections.ContainsKey('.interrupts') -or
        $sections['.interrupts'].Vma -ne $image.Vector) {
        $failures.Add("$($image.Label): unexpected vector address")
    }
    foreach ($entry in $sections.GetEnumerator()) {
        $section = $entry.Value
        if ($section.Size -eq 0) { continue }
        if (($entry.Key -in $flashSections) -and
            ($section.Lma -ge $image.Start) -and ($section.Lma -lt $image.End) -and
            (($section.Lma + $section.Size) -gt $image.End)) {
            $failures.Add("$($image.Label): $($entry.Key) exceeds Flash limit")
        }
        if (($entry.Key -in $ramSections) -and
            ($section.Vma -ge 0x1FFF8000L) -and ($section.Vma -lt 0x20006FF0L) -and
            (($section.Vma + $section.Size) -gt 0x20006FF0L)) {
            $failures.Add("$($image.Label): $($entry.Key) exceeds SRAM limit")
        }
    }
    Write-Output ('PASS {0}: vector=0x{1:X8}, limit=0x{2:X8}' -f
                  $image.Label, $image.Vector, $image.End)
}

if ($failures.Count -ne 0) {
    $failures | ForEach-Object { Write-Error "FAIL $_" }
    exit 1
}

Write-Output 'PASS V2 layout: no checked Flash/RAM overlap'
