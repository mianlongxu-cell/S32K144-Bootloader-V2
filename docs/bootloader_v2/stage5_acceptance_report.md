# Stage5 Hardware Acceptance Report

Status: PASS — all eleven listed functional acceptance items; button/reset mapping remains a separate follow-up.  
Acceptance host date: 2026-09-19  
Target: NXP S32K144F512M15  
Debugger: P&E OpenSDA / S32 Design Studio 3.6.8  
Gateway: Ubuntu SocketCAN `can0`, 500 kbit/s

## Preserved Stage4 baseline

- Final active image: Slot B, version 13.0.0.0.
- Inactive image: Slot A, version 12.0.0.0.
- Stage4 Journal: transaction 18, `PENDING_ACTIVATION/SUCCESS`, target B.
- Full PFlash backup: `Application_Build/stage4_final_pflash.bin`.
- PFlash size: 524288 bytes.
- PFlash SHA-256: `259EDFA4EB018CB4A33C199EE828D79454DB5F2709A40D9BF9D40C93A84CEFD0`.
- Slot A was byte-identical to the packed A12 image and Slot B was byte-identical to the packed B13 image before Stage5 work began.

## Software acceptance

Command:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build_stage5.ps1 -VersionA 14.0.0 -VersionB 15.0.0
```

Result: PASS

- Stage2 host tests: 25 passed.
- Stage3 production programming/UDS suite: passed.
- Stage4 production Journal suite: 792 assertions passed.
- Stage4 production ISO-TP suite: 11 cases passed.
- Stage5 lifecycle suite: 382 assertions passed.
- Linux gateway unit tests: 50 passed.
- Stage5 layout/RAM-driver checks: passed.
- Release and FaultInjection Bootloaders built successfully.

## Board image plan

| Image | Purpose | Version |
|---|---|---:|
| Stage5 normal Slot A | Stage4-to-Stage5 migration and normal confirmation | 14.0.0.0 |
| Stage5 normal Slot B | Reserved normal image | 15.0.0.0 |
| TestMode 1 Slot B | Trial does not confirm | 15.0.0.0 |
| TestMode 2 Slot B | Watchdog reset | 16.0.0.0 |
| TestMode 3 Slot B | HardFault reset | 17.0.0.0 |
| TestMode 4 Slot B | Software reset loop | 18.0.0.0 |
| TestMode 5 Slot B | Delayed confirmation | 19.0.0.0 |

The version sequence is strictly newer than the preserved A12/B13 baseline. Test modes are used one at a time after rollback to the confirmed Slot A image.

## Hardware acceptance log

Pre-install hardware baseline verified on 2026-09-20:

- F101 returned `01`: Slot B active.
- F102 returned `0D000000`: version 13.0.0.0.
- Evidence: `evidence/stage5/01_stage4_baseline_slot_b.png` and
  `evidence/stage5/02_stage4_baseline_version_v13.png`.

Stage5 migration image staged with the Stage4 Release Bootloader:

- Linux copy SHA-256 matched the manifest for the A14 packed image.
- Full B13 to A14 update passed erase, programming, transfer-exit, image verification, reset, and post-reset identity checks.
- Evidence: `evidence/stage5/04_stage4_flash_stage5_a14_success.png`.
- Cold boot identity passed: F101 `00`, F102 `0E000000`.
- Pre-Stage5-install full PFlash backup: `Application_Build/pre_stage5_bootloader_install_pflash.bin`.
- Backup size: 524288 bytes.
- Backup SHA-256: `66DD8812D85A78B24811FD7488EBB26068B7C6EE1F6A040485DBB15E0910E3FC`.
- Backup Slot A is byte-identical to Stage5 A14; Slot B is byte-identical to Stage4 B13.

Stage5 installation and migration result:

- P&E verified the installed Stage5 Release Bootloader without reprogramming; cumulative CRC-16 was `$EDDE`.
- A14 cold-booted and completed the Trial health-confirmation handshake.
- F105 reported both A and B Metadata valid and `CONFIRMED`, Active A, Attempts 0, Last Reset `SOFTWARE`, Last Boot Result `CONFIRMED`.
- F104 Journal converged to `IDLE/SUCCESS`, transaction 19.
- Linux F105 rendering initially decoded `0x0E000000` as `3584.0.0`. The decoder was corrected to four-byte version formatting, regression assertions were added, and all 50 Linux tests passed again.

Watchdog raw Metadata evidence:

- Dump: `Application_Build/stage5_watchdog_metadata.bin`, 16384 bytes.
- SHA-256: `BA1219C0C8CEA1C98B7E5FD276B596BB3571DC72F30EEC6A0B1A9614DBFA6B59`.
- All four A0/A1/B0/B1 records had valid CRC and commit markers.
- Latest B record: B0, sequence 13, version 16.0.0.0, `INVALID`, Attempts 3.
- Persistent `last_reset_reason=2` (`WATCHDOG`), `ROLLBACK_OCCURRED`, failed slot B, rollback target A.

HardFault raw Metadata evidence:

- Dump: `Application_Build/stage5_hardfault_metadata.bin`, 16384 bytes.
- SHA-256: `4F1F4C165E5AB20441E1B343CB5CAF89DD552359CD57ECFB1050B34AE0C7E47C`.
- All four A0/A1/B0/B1 records had valid CRC and commit markers.
- Latest B record: B0, sequence 19, version 17.0.0.0, `INVALID`, Attempts 3.
- Persistent `last_reset_reason=5` (`FAULT`), `ROLLBACK_OCCURRED`, failed slot B, rollback target A.

Software reset-loop raw Metadata evidence:

- Dump: `Application_Build/stage5_resetloop_metadata.bin`, 16384 bytes.
- SHA-256: `2BABC309BC709C1A44A1CD3270CD22F103F3FA857035497D5C4C1B536BFF4169`.
- All four A0/A1/B0/B1 records had valid CRC and commit markers.
- Latest B record: B0, sequence 25, version 18.0.0.0, `INVALID`, Attempts 3.
- Persistent `last_reset_reason=3` (`SOFTWARE`), `ROLLBACK_OCCURRED`, failed slot B, rollback target A.

Delayed-confirm real power-loss Metadata evidence:

- Dump: `Application_Build/stage5_delayconfirm_metadata.bin`, 16384 bytes.
- SHA-256: `E8F0276A5D231D0592D3D52856F67E621EEE9C16026A5318A2DA9BB6813C05AD`.
- All four A0/A1/B0/B1 records had valid CRC and commit markers.
- B0 sequence 29 preserved version 19.0.0.0 as `TRIAL`, Attempts 2, `last_reset_reason=1` (`POWER_ON`).
- B1 sequence 30 committed version 19.0.0.0 as `CONFIRMED`, Attempts 0, successful boots 1.

| ID | Test | Expected | Result | Evidence |
|---:|---|---|---|---|
| 1 | Preserve Stage4 baseline and install Stage5 | A/B/Journal preserved | PASS | Full backups and P&E CRC `$EDDE` |
| 2 | Pending first boot | target becomes Trial attempt 1 | PASS | B15 Trial, Attempts 1, Active B, Journal IDLE |
| 3 | Normal health confirmation | A14 becomes Confirmed attempt 0 | PASS | F105 valid/Confirmed, Active A, Attempts 0 |
| 4 | NO_CONFIRM Trial | attempt increments; after 3 failures B invalid and rollback A | PASS | Attempts 1/2/3; B15 INVALID; ROLLBACK_OCCURRED; cold-booted A14 |
| 5 | Watchdog Trial | reset reason WATCHDOG; attempt increments/rollback | PASS | Raw B0 Metadata reason WATCHDOG; B16 INVALID/3; rollback A14 |
| 6 | HardFault Trial | reset reason FAULT; attempt increments/rollback | PASS | Raw B0 Metadata reason FAULT; B17 INVALID/3; rollback A14 |
| 7 | Reset-loop Trial | software reset attempts and rollback | PASS | Raw B0 Metadata reason SOFTWARE; B18 INVALID/3; rollback A14 |
| 8 | Delayed-confirm Trial power cut | POWER_ON increments attempt; second Trial confirms | PASS | Raw B0 Trial/2/POWER_ON, B1 Confirmed/0; active B19 |
| 9 | F105 ISO-TP response | complete 40-byte payload parsed | PASS | Repeated 40-byte F105 parses during all lifecycle scenarios |
| 10 | Final Release image and cold boot | production Bootloader; stable confirmed image | PASS | Following the SBC/TCR fix, A22 and B23 each passed debugger-disconnected cold-boot slot/version queries; both CONFIRMED/0, Active B. Evidence 58–64. Historical B21 CAN failure retained below. |
| 11 | Final PFlash evidence | exact slot images and metadata captured | PASS | 512 KiB dump; Release SREC data matched, A22/B23 slots byte-identical, image CRCs valid; both Journal and all four Metadata copies valid. Latest Journal transaction 28 IDLE/SUCCESS; latest A/B CONFIRMED/0. |

## Scope note

Observation during NO_CONFIRM testing: one manual board-button reset left the green LED on with no application CAN traffic until a full power cycle. Metadata remained valid and the lifecycle subsequently reached B15 `INVALID`, Attempts 3, with rollback target A. This observation must be distinguished between an attached debugger/reset-catch condition and the board's physical reset/boot-request button mapping before closing the report.

Final cold-boot diagnosis: the MCU receive and diagnostic paths are alive, while the
physical transmit path is silent. The target is an S32K144EVB whose CAN physical layer
uses a UJA1169 SBC controlled through LPSPI1. Neither the Bootloader nor Application
currently initializes that SBC. The observed one-way behavior after POR is therefore
consistent with the UJA1169 remaining in Standby/silent operation after power-up. This
must be confirmed by reading its SPI status, then fixed by explicitly placing it in
Normal mode before FlexCAN traffic is accepted as production-ready.

SBC fix candidate (pending board retest):

- Added bounded, polling LPSPI1 access to UJA1169 on PTB14..PTB17/PCS3.
- Reads device ID, Main/Watchdog/SBC configuration, supply and transceiver status.
- If not already in Forced Normal mode, enables V2 for Normal mode, selects CAN Active
  with undervoltage monitoring and commands SBC Normal mode.
- Does not write the UJA1169 non-volatile configuration area.
- Application runs the sequence on every start; Bootloader runs it before starting its
  programming CAN server.
- Full Stage2 through Stage5 and Linux regression passed after the change: Stage2 25,
  Stage4 792, ISO-TP 11, Stage5 lifecycle 382, Linux 50 tests.
- Initial candidate board test read `0xFF` from all UJA registers, while a later manual
  transaction returned device ID `0xEF`. This proved the SPI pins and SBC were healthy
  and exposed access before the UJA1169 post-reset `tto(SPI)` interval had elapsed.
- The revised candidate adds a conservative post-reset wait, five bounded ID retries,
  and a post-CAN-Active settling delay before checking CTS.
- The revised candidate entered the Bootloader programming server, but its five ID
  reads returned `0x00` (`Uja1169InitResult=BAD_DEVICE`). A no-reset attach showed
  `LPSPI1_TCR=0x0000001F`; the intended `0x4300000F` command had been written while
  `MEN=0` and was ignored. Repeating the same transaction after enabling LPSPI1
  returned `0xFDEF` (device ID `0xEF`), proving the SBC and wiring were healthy.
  Evidence: `53_new_bootloader_noreset_attach.png`,
  `54_new_bootloader_uja_bad_device_zero.png`, and
  `55_uja_manual_spi_id_ef_tcr_root_cause.png`.
- The final candidate writes TCR only after setting MEN. Full Stage2 through Stage5
  and Linux regression passed again: Stage2 25, Stage4 792, ISO-TP 11, Stage5
  lifecycle 382, Linux 50 tests.
- Final candidate images: Release Bootloader ELF SHA-256
  `0E3396D98D7256E6A7EE816BC08BA64B6804052E7BCBC1C316ED1BCDBAFFA55C`,
  Release Bootloader SREC `E4C44E7CB72A593805E8E611FA98A87B38B4F6AC5F29065691C3DF433B38D09B`,
  Slot A V22 `B4A384934B269D4004AAE9CA2E83DCCD15DB63351176AF721545D4B33939AB29`,
  Slot B V23 `71634015D270914E296D3055ED739188E9732CB2AFBA049A7FFAE4075CD1EFCE`.

TCR fix board verification: UJA device ID was `0xEF` on the first read,
InitResult was 1 (Forced Normal), and TCR was `0x4300000F`. With the target
running, F101 and F102 requests both received positive responses on CAN ID
`0x7E8`, reporting Slot B and version 21. Host counters showed two transmitted
and two received frames, zero bus errors, and nine RX drops of undetermined
origin. Evidence: `56_tcrfix_bootloader_can_responses.png` and
`57_tcrfix_host_can_statistics.png`. This verifies Bootloader CAN communication;
The fixed A22 application subsequently ran and responded to version queries.
The flasher initially reported a post-reset confirmation timeout while the debugger
had halted the target. After resuming and entering the Bootloader programming server,
F105 reported A22 CONFIRMED, Attempts 0, Active A, both metadata records valid,
and Last Boot Result CONFIRMED (`58_a22_confirmed.png`). F105 is implemented in
the Bootloader; querying it in the application returns NRC 0x31 as configured.
Following the instructed debugger-disconnected power cycle, F101 returned Slot A
and F102 returned version 22 (`59_a22_cold_boot_slot.png`,
`60_a22_cold_boot_version.png`). This single A22 cold-boot CAN check passed.
B23 update passed erase, all 1792 transfer blocks, TransferExit, image verification,
and post-reset identification as Slot B V23.0.0.0. The flasher reported SUCCESS
(`61_b23_flash_success.png`). Its initial A22 CONFIRMED/B21 CONFIRMED status
describes the pre-update state, not B23 lifecycle confirmation.
B23 also responded after the instructed debugger-disconnected power cycle: F101
returned Slot B and F102 returned version 23 (`62_b23_cold_boot_slot.png`,
`63_b23_cold_boot_version.png`). This single B23 cold-boot CAN check passed.
After entering the programming server through UDS 10 02, F105 reported A22 and
B23 both CONFIRMED, Attempts 0, Metadata VALID, Active B and Last Boot Result
CONFIRMED (`64_b23_final_confirmed.png`). Last Reset SOFTWARE reflects that
programming-entry reset, not the preceding cold power cycle.

## Final PFlash verification

- Dump: `Application_Build/stage5_final_pflash.bin`, exactly 524288 bytes.
- SHA-256: `A073C94E3F01FA93AF8927CA6562C93E0CDB83A1174A20DD55833E07E6430BCF`.
- All 27164 data bytes in the Release Bootloader SREC match the dump at their
  specified addresses. Unspecified SREC gaps are not included in this comparison.
- Complete 229376-byte Slot A and Slot B regions match the fixed A22/B23 packed
  images byte-for-byte; header and payload CRCs pass independently.
- Journal copies: sequence 329 PENDING_ACTIVATION and sequence 330 IDLE, both
  valid, transaction 28. Latest result SUCCESS, target B23, 229376 bytes committed.
  Journal active_slot=A describes the update source, not the final running slot.
- A0 sequence 11 TRIAL/1, A1 sequence 12 CONFIRMED/0; B0 sequence 37 TRIAL/1,
  B1 sequence 38 CONFIRMED/0. All four records have valid CRC/commit markers and
  fields. Latest versions are A22 and B23, each with one successful confirmation.
- Older TRIAL copies record reset reason WATCHDOG (2); latest CONFIRMED copies
  record SOFTWARE (3). These are persisted lifecycle transition reasons, not
  evidence of the subsequent cold-boot reset source.
- Reproduce the read-only check from the project root with
  `node tools/verify_stage5_final.js`.
- Historical failure: B21 ran in FreeRTOS idle but did not deliver CAN responses
  to the host (evidence 42–45). The added SBC initialization and subsequent TCR
  ordering correction were verified by the final A22/B23 hardware checks.
- All eleven listed functional acceptance items now pass. The button/reset
  mapping observation in the scope note remains a separate unresolved follow-up.

This is functional hardware acceptance, not an automotive safety qualification. Temperature, voltage-ramp, repeated endurance, EMC, and deliberate PFlash ECC fault campaigns remain outside this report unless separately recorded.
