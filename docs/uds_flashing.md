# UDS firmware flashing

## Programming sequence

The Linux flasher uses the physical CAN IDs `0x7E0/0x7E8` and performs:

1. `10 02` to the running Application;
2. wait for software reset and Bootloader programming session;
3. `10 02` to the Bootloader;
4. `27 01` seed request and `27 02 <key>` unlock;
5. `31 01 FF00 <address> <size>` erase;
6. `34 00 44 <address:4> <size:4>` request download;
7. `36 <BSC> <data>` for all blocks;
8. `37` transfer exit;
9. `31 01 FF01 <size> <CRC32> <version>` verify and mark valid;
10. `11 01` reset and wait for DID `0xF100`.

## Download context

The Bootloader keeps a bounded context containing the accepted address, size,
write offset, expected block sequence counter, and transfer state. Erase,
download, transfer, exit, and verify requests are rejected when called out of
sequence.

The negotiated maximum block length is 130 bytes: two UDS bytes (SID and block
counter) plus at most 128 firmware bytes. ISO-TP segments these UDS requests into
First Frame and Consecutive Frames as required.

## Block sequence counter

The first TransferData block uses BSC 1. Each accepted block echoes its BSC in
the positive `0x76` response, and the expected counter increments with 8-bit
wraparound. An unexpected value returns NRC `0x73` and no bytes are programmed
for that request.

## SecurityAccess

Erase and download require a successful seed/key exchange. The demonstration
algorithm uses a project-specific XOR and rotate transform. It demonstrates
state gating but is not production cryptography. Requests that skip unlock are
rejected with NRC `0x33`.

## Address and Flash protection

- accepted download start is within the Application region;
- length must be nonzero and cannot overflow the configured end;
- sector erase and phrase program operations validate their address ranges;
- Bootloader and metadata addresses cannot be selected by RequestDownload;
- each programmed phrase is read back before the transfer continues.

## CRC and metadata commit

The metadata is invalidated before the Application erase. After TransferExit,
the verify routine recalculates CRC32 over the exact received firmware size. A
mismatch returns an error and leaves the image invalid. Only a matching size and
CRC cause the valid metadata marker to be programmed.

## Recovery behavior

An interrupted transfer, bad CRC, wrong BSC, or other programming failure leaves
the Application invalid. On reset the Bootloader remains available instead of
jumping into a partial image. A subsequent complete download can erase and
recover the ECU.

## Linux command

```bash
cd linux_gateway
./uds_flasher ../firmware/app_v2.bin --channel can0
```

Validated fault-injection switches are implemented by the real tool:

```bash
./uds_flasher ../firmware/app_v2.bin --stop-at 30
./uds_flasher ../firmware/app_v2.bin --wrong-crc
./uds_flasher ../firmware/app_v2.bin --wrong-sequence
./uds_flasher ../firmware/app_v2.bin --address 0
./uds_flasher ../firmware/app_v2.bin --skip-security
```

These options are for bench validation, not production updates.
