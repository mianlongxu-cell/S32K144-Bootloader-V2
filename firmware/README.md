# Firmware release assets

Firmware binaries are generated locally and ignored by Git:

- `app_v1.bin`
- `app_v2.bin`

For a public release, rebuild each image from the corresponding tagged source,
record its SHA-256 checksum, and attach it to the GitHub Release only after the
redistribution rights of all linked third-party components have been verified.

The Linux flasher accepts a raw Application image linked for address `0x8000`:

```bash
./linux_gateway/uds_flasher firmware/app_v2.bin
```
