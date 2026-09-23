'use strict';

const fs = require('fs');

const HEADER_MAGIC = 0x42493256;
const HEADER_VERSION = 1;
const HEADER_SIZE = 64;
const HEADER_CRC_LENGTH = 60;
const SRAM_START = 0x1FFF8000;
const SRAM_END = 0x20006FF0;

const SLOTS = Object.freeze({
  A: Object.freeze({
    id: 0,
    name: 'A',
    baseAddress: 0x00008000,
    size: 0x00038000,
    vectorAddress: 0x00008000,
    headerAddress: 0x0003F000,
    payloadEndAddress: 0x0003F000,
  }),
  B: Object.freeze({
    id: 1,
    name: 'B',
    baseAddress: 0x00040000,
    size: 0x00038000,
    vectorAddress: 0x00040000,
    headerAddress: 0x00077000,
    payloadEndAddress: 0x00077000,
  }),
});

function crc32(data) {
  let crc = 0xFFFFFFFF;
  for (const byte of data) {
    crc = (crc ^ byte) >>> 0;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = ((crc & 1) !== 0)
        ? ((crc >>> 1) ^ 0xEDB88320) >>> 0
        : (crc >>> 1) >>> 0;
    }
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function encodeVersion(major, minor, patch, build = 0) {
  for (const [name, value] of Object.entries({ major, minor, patch, build })) {
    if (!Number.isInteger(value) || value < 0 || value > 255) {
      throw new Error(`${name} must be an integer in 0..255`);
    }
  }
  return (((major << 24) | (minor << 16) | (patch << 8) | build) >>> 0);
}

function parseVersion(text) {
  const match = /^(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$/.exec(text);
  if (!match) {
    throw new Error('version must be major.minor.patch[.build]');
  }
  const parts = match.slice(1).map((value) => value === undefined ? 0 : Number(value));
  return encodeVersion(parts[0], parts[1], parts[2], parts[3]);
}

function compareVersion(left, right) {
  const a = left >>> 0;
  const b = right >>> 0;
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

function slotFor(value) {
  const name = String(value).toUpperCase();
  if (!Object.prototype.hasOwnProperty.call(SLOTS, name)) {
    throw new Error('slot must be A or B');
  }
  return SLOTS[name];
}

function containsRange(slot, address, length) {
  if (!Number.isInteger(address) || !Number.isInteger(length) || length <= 0) {
    return false;
  }
  const end = slot.baseAddress + slot.size;
  return address >= slot.baseAddress && address < end && length <= end - address;
}

function buildHeader({ payload, slot, softwareVersion, buildId, flags = 0 }) {
  const imageSize = payload.length;
  const imageCrc = crc32(payload);
  const initialMsp = payload.readUInt32LE(0);
  const entryAddress = payload.readUInt32LE(4);
  const header = Buffer.alloc(HEADER_SIZE, 0);
  const fields = [
    HEADER_MAGIC,
    HEADER_VERSION,
    imageSize,
    imageCrc,
    softwareVersion >>> 0,
    (buildId === undefined ? imageCrc : buildId) >>> 0,
    slot.vectorAddress,
    entryAddress,
    flags >>> 0,
  ];
  fields.forEach((value, index) => header.writeUInt32LE(value >>> 0, index * 4));
  header.writeUInt32LE(crc32(header.subarray(0, HEADER_CRC_LENGTH)), 60);
  return { header, imageSize, imageCrc, initialMsp, entryAddress };
}

function validatePayloadForSlot(payload, slot) {
  const maximum = slot.payloadEndAddress - slot.vectorAddress;
  if (!Buffer.isBuffer(payload) || payload.length < 8 || payload.length > maximum) {
    throw new Error(`payload size must be 8..${maximum} bytes for Slot ${slot.name}`);
  }
  const msp = payload.readUInt32LE(0);
  const reset = payload.readUInt32LE(4);
  const resetAddress = (reset & 0xFFFFFFFE) >>> 0;
  if (msp < SRAM_START || msp > SRAM_END || (msp & 7) !== 0) {
    throw new Error(`invalid initial MSP 0x${msp.toString(16).padStart(8, '0')}`);
  }
  if ((reset & 1) === 0 || resetAddress < slot.vectorAddress ||
      resetAddress >= slot.vectorAddress + payload.length) {
    throw new Error(`invalid Reset Handler 0x${reset.toString(16).padStart(8, '0')}`);
  }
}

function packImage({ payload, slot: slotValue, softwareVersion, buildId, flags = 0 }) {
  const slot = typeof slotValue === 'string' ? slotFor(slotValue) : slotValue;
  validatePayloadForSlot(payload, slot);
  const built = buildHeader({ payload, slot, softwareVersion, buildId, flags });
  const image = Buffer.alloc(slot.size, 0xFF);
  payload.copy(image, slot.vectorAddress - slot.baseAddress);
  built.header.copy(image, slot.headerAddress - slot.baseAddress);
  return { image, slot, ...built };
}

function readHeader(image, slotValue) {
  const slot = typeof slotValue === 'string' ? slotFor(slotValue) : slotValue;
  const offset = slot.headerAddress - slot.baseAddress;
  if (!Buffer.isBuffer(image) || image.length !== slot.size) {
    throw new Error(`image must be exactly ${slot.size} bytes`);
  }
  const header = image.subarray(offset, offset + HEADER_SIZE);
  const values = [];
  for (let offsetIndex = 0; offsetIndex < HEADER_SIZE; offsetIndex += 4) {
    values.push(header.readUInt32LE(offsetIndex));
  }
  return {
    raw: header,
    magic: values[0],
    headerVersion: values[1],
    imageSize: values[2],
    imageCrc32: values[3],
    softwareVersion: values[4],
    buildId: values[5],
    vectorAddress: values[6],
    entryAddress: values[7],
    flags: values[8],
    headerCrc32: values[15],
  };
}

function validatePackedImage(image, slotValue) {
  const slot = typeof slotValue === 'string' ? slotFor(slotValue) : slotValue;
  let header;
  try {
    header = readHeader(image, slot);
  } catch (_) {
    return 'ERR_SIZE';
  }
  if (header.magic !== HEADER_MAGIC) return 'ERR_MAGIC';
  if (header.headerVersion !== HEADER_VERSION) return 'ERR_HEADER_VERSION';
  if (header.imageSize === 0 ||
      header.imageSize > slot.payloadEndAddress - slot.vectorAddress) return 'ERR_SIZE';
  if (crc32(header.raw.subarray(0, HEADER_CRC_LENGTH)) !== header.headerCrc32) {
    return 'ERR_HEADER_CRC';
  }
  if (header.vectorAddress !== slot.vectorAddress ||
      !containsRange(slot, header.vectorAddress, 8)) return 'ERR_VECTOR';
  const payload = image.subarray(0, header.imageSize);
  const msp = payload.readUInt32LE(0);
  const reset = payload.readUInt32LE(4);
  const resetAddress = (reset & 0xFFFFFFFE) >>> 0;
  if (msp < SRAM_START || msp > SRAM_END || (msp & 7) !== 0) {
    return 'ERR_STACK_POINTER';
  }
  if ((reset & 1) === 0 || resetAddress < slot.vectorAddress ||
      resetAddress >= slot.vectorAddress + header.imageSize) return 'ERR_RESET_HANDLER';
  if (header.entryAddress !== reset) return 'ERR_ENTRY_MISMATCH';
  if (crc32(payload) !== header.imageCrc32) return 'ERR_IMAGE_CRC';
  return 'VALID';
}

function selectTarget(a, b) {
  if (a.valid && !b.valid) return 'SLOT_A';
  if (!a.valid && b.valid) return 'SLOT_B';
  if (!a.valid && !b.valid) return 'PROGRAMMING';
  return compareVersion(a.version, b.version) < 0 ? 'SLOT_B' : 'SLOT_A';
}

function parseUnsigned(text, name) {
  const value = Number(text);
  if (!Number.isInteger(value) || value < 0 || value > 0xFFFFFFFF) {
    throw new Error(`${name} must be a uint32 value`);
  }
  return value >>> 0;
}

function writeManifest(path, details) {
  fs.writeFileSync(path, `${JSON.stringify(details, null, 2)}\n`);
}

module.exports = {
  HEADER_MAGIC,
  HEADER_VERSION,
  HEADER_SIZE,
  HEADER_CRC_LENGTH,
  SLOTS,
  crc32,
  encodeVersion,
  parseVersion,
  compareVersion,
  slotFor,
  containsRange,
  packImage,
  readHeader,
  validatePackedImage,
  selectTarget,
  parseUnsigned,
  writeManifest,
};
