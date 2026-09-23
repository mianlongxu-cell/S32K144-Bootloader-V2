#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
FIRMWARE="${1:-}"
CHANNEL="${2:-can0}"

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware not found: ${FIRMWARE}" >&2
    echo "Usage: $0 <inactive-slot packed_image.bin> [can-channel]" >&2
    exit 2
fi

exec python3 "${GATEWAY_DIR}/uds_flasher" "${FIRMWARE}" --channel "${CHANNEL}"
