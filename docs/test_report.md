# Bench test report

Platform: NXP S32K144EVB, Classical CAN at 500 kbit/s, USB-CAN/PCAN-compatible
adapter, Linux SocketCAN. Results below record tests actually completed during
the complete system bench validation; no unperformed measurements are inferred.

| Test ID | Description | Expected result | Result |
|---|---|---|---|
| TC01 | Normal boot | valid metadata/image starts Application | PASS |
| TC02 | Application boot request | `10 02` causes reset into Bootloader | PASS |
| TC03 | Invalid/incomplete Application | Bootloader does not jump; programming remains available | PASS |
| TC04 | V1.0 to V2.0 update | full update succeeds and DID `F100` returns `V2.0` | PASS |
| TC05 | Correct CRC verify | verify routine succeeds before metadata commit | PASS |
| TC06 | Interrupted update (`--stop-at 30`) | partial image remains invalid; normal reflash recovers | PASS |
| TC07 | Invalid CRC (`--wrong-crc`) | CRC verify fails; image is not started; reflash recovers | PASS |
| TC08 | Wrong TransferData sequence | NRC `0x73`/transfer failure; image not committed | PASS |
| TC09 | Invalid address (`--address 0`) | Bootloader range is rejected and not erased | PASS |
| TC10 | Programming while locked | erase/download rejected with NRC `0x33` | PASS |
| TC11 | Reset and power-cycle recovery | V2.0 starts and remains readable after reset/power cycle | PASS |
| TC12 | Periodic CAN after update | `0x100/0x101/0x102` continue at nominal periods | PASS |
| TC13 | SocketCAN health | 500 kbit/s, ERROR-ACTIVE, no observed bus-off in acceptance capture | PASS |
| TC14 | VIN multi-frame response | Linux sends FC and reassembles VIN | PASS |
| TC15 | DTC read/clear | 24-bit DTC/status decoded and clear response accepted | PASS |

## Recorded periodic timing

The acceptance capture showed approximately:

- `0x100`: 100 ms;
- `0x101`: 200 ms;
- `0x102`: 500 ms.

The final SocketCAN status capture reported zero RX/TX errors, zero dropped
frames, and zero bus-off count for that run.

## Unit tests

The Linux unit suite contains 22 tests covering CAN frame encoding, vehicle
decoding, CSV logging, control encoding, ISO-TP SF/FF/CF/FC behavior, UDS
responses, CRC helper, and seed/key helper. The suite passed during final
system validation.

## Limitations

- This is a bench prototype, not a production safety/security assessment.
- SecurityAccess is demonstrative and not cryptographically strong.
- Flash endurance, EMC, long-duration soak, malformed-frame fuzzing, and
  multi-node bus-load qualification were not claimed as tested.
- CAN FD, secure boot, signatures, encryption, and A/B slots are out of scope.
