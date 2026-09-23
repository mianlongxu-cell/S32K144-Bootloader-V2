# Application UDS diagnostics

## Transport

- Classical CAN, 500 kbit/s
- physical request ID: `0x7E0`
- physical response ID: `0x7E8`
- maximum diagnostic PDU: 256 bytes
- ECU receive flow control: block size 0, STmin 0 ms
- N_Bs/N_Cr timeout: 1000 ms

## Supported services

Only services present in `src/Dcm.c` are listed.

| SID | Service | Supported request | Positive response |
|---:|---|---|---|
| `0x10` | DiagnosticSessionControl | `10 01`, `10 03`, `10 02` | `50 <session> ...` |
| `0x3E` | TesterPresent | `3E 00` | `7E 00` |
| `0x22` | ReadDataByIdentifier | one configured DID | `62 <DID> <data>` |
| `0x19` | ReadDTCInformation | `19 02 <statusMask>` | `59 02 <availability> <records>` |
| `0x14` | ClearDiagnosticInformation | `14 FF FF FF` | `54` |

`10 02` is the Application-side programming request. After its positive
response, the Application writes a retained SRAM boot request and performs a
software reset so the Bootloader programming server becomes active.

## Configured DIDs

| DID | Name | Encoding |
|---:|---|---|
| `0xF190` | VIN | ASCII, multi-frame response (`TESTS32K144000001`) |
| `0xF100` | Software version | ASCII (`V1.0` or `V2.0`) |
| `0x0101` | Vehicle speed | unsigned 16-bit little-endian, km/h |
| `0x0102` | Engine RPM | unsigned 16-bit little-endian |
| `0x0103` | Coolant temperature | unsigned 8-bit, degrees C |
| `0x0104` | Gear | unsigned 8-bit |

The `0x0101..0x0104` and `0xF100` DIDs are project-specific learning DIDs; they
are not claimed as standardized vehicle DIDs.

## DTC reporting

`19 02` returns 24-bit DTC codes followed by one status byte. The Linux tester
decodes the implemented status bits:

- bit 0: `testFailed`;
- bit 2: `pendingDTC`;
- bit 3: `confirmedDTC`.

Validated examples include coolant over-temperature `0x000101` and control
heartbeat timeout `0x000301`. `14 FF FF FF` clears all stored project DTCs; an
active fault condition can set the DTC again on subsequent monitor cycles.

## Negative responses

The Application currently uses:

- `0x11` ServiceNotSupported;
- `0x12` SubFunctionNotSupported;
- `0x13` IncorrectMessageLengthOrInvalidFormat;
- `0x31` RequestOutOfRange.
