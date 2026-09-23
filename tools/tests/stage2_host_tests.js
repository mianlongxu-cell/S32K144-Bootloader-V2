#!/usr/bin/env node
'use strict';

const assert = require('assert');
const format = require('../image_format');

let passed = 0;

function test(name, callback) {
  callback();
  passed += 1;
  console.log(`PASS ${name}`);
}

function fixture(slotName, version = '1.0.0') {
  const slot = format.slotFor(slotName);
  const payload = Buffer.alloc(512, 0xA5);
  payload.writeUInt32LE(0x20006FF0, 0);
  payload.writeUInt32LE((slot.vectorAddress + 0x101) >>> 0, 4);
  return format.packImage({
    payload,
    slot,
    softwareVersion: format.parseVersion(version),
  });
}

function corrupt(packed, mode) {
  const image = Buffer.from(packed.image);
  const slot = packed.slot;
  const headerOffset = slot.headerAddress - slot.baseAddress;
  const header = format.readHeader(image, slot);
  const refreshHeaderCrc = () => image.writeUInt32LE(
    format.crc32(image.subarray(headerOffset, headerOffset + 60)),
    headerOffset + 60);
  const refreshPayloadAndHeaderCrc = () => {
    image.writeUInt32LE(format.crc32(image.subarray(0, header.imageSize)), headerOffset + 12);
    refreshHeaderCrc();
  };
  if (mode === 'magic') image.writeUInt32LE((header.magic ^ 1) >>> 0, headerOffset);
  if (mode === 'header-crc') image.writeUInt32LE((header.headerCrc32 ^ 1) >>> 0, headerOffset + 60);
  if (mode === 'image-crc') image[8] ^= 1;
  if (mode === 'vector') {
    image.writeUInt32LE(slot.vectorAddress + 0x400, headerOffset + 24);
    refreshHeaderCrc();
  }
  if (mode === 'reset-handler') {
    image.writeUInt32LE(0xFFFFFFFF, 4);
    image.writeUInt32LE(format.crc32(image.subarray(0, header.imageSize)), headerOffset + 12);
    image.writeUInt32LE(0xFFFFFFFF, headerOffset + 28);
    refreshHeaderCrc();
  }
  if (mode === 'msp') {
    image.writeUInt32LE(0, 0);
    refreshPayloadAndHeaderCrc();
  }
  if (mode === 'entry-mismatch') {
    image.writeUInt32LE((slot.vectorAddress + 0x201) >>> 0, headerOffset + 28);
    refreshHeaderCrc();
  }
  if (mode === 'header-version') {
    image.writeUInt32LE(2, headerOffset + 4);
    refreshHeaderCrc();
  }
  if (mode === 'size') {
    image.writeUInt32LE((slot.payloadEndAddress - slot.vectorAddress + 1) >>> 0, headerOffset + 8);
    refreshHeaderCrc();
  }
  return image;
}

test('CRC32 known vector 123456789', () => {
  assert.strictEqual(format.crc32(Buffer.from('123456789')), 0xCBF43926);
});
test('version 1.0.0 < 1.1.0', () => {
  assert(format.compareVersion(format.parseVersion('1.0.0'), format.parseVersion('1.1.0')) < 0);
});
test('version 1.9.0 < 1.10.0', () => {
  assert(format.compareVersion(format.parseVersion('1.9.0'), format.parseVersion('1.10.0')) < 0);
});
test('version 1.10.0 < 2.0.0', () => {
  assert(format.compareVersion(format.parseVersion('1.10.0'), format.parseVersion('2.0.0')) < 0);
});
test('equal versions compare equal', () => {
  assert.strictEqual(format.compareVersion(format.parseVersion('1.10.0'), format.parseVersion('1.10.0')), 0);
});

const a = fixture('A');
const b = fixture('B');
test('Slot A packed image validates', () => assert.strictEqual(format.validatePackedImage(a.image, 'A'), 'VALID'));
test('Slot B packed image validates', () => assert.strictEqual(format.validatePackedImage(b.image, 'B'), 'VALID'));
test('header is exactly 64 bytes', () => assert.strictEqual(a.header.length, 64));
test('bad magic rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'magic'), 'A'), 'ERR_MAGIC'));
test('bad header CRC rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'header-crc'), 'A'), 'ERR_HEADER_CRC'));
test('bad image CRC rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'image-crc'), 'A'), 'ERR_IMAGE_CRC'));
test('bad vector rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'vector'), 'A'), 'ERR_VECTOR'));
test('bad Reset Handler rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'reset-handler'), 'A'), 'ERR_RESET_HANDLER'));
test('bad MSP rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'msp'), 'A'), 'ERR_STACK_POINTER'));
test('entry mismatch rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'entry-mismatch'), 'A'), 'ERR_ENTRY_MISMATCH'));
test('unsupported header version rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'header-version'), 'A'), 'ERR_HEADER_VERSION'));
test('oversize payload rejected', () => assert.strictEqual(format.validatePackedImage(corrupt(a, 'size'), 'A'), 'ERR_SIZE'));
test('slot range accepts final byte', () => assert(format.containsRange(format.SLOTS.A, 0x0003FFFF, 1)));
test('slot range rejects crossing end', () => assert(!format.containsRange(format.SLOTS.A, 0x0003FFFF, 2)));

test('policy A valid/B invalid selects A', () => assert.strictEqual(format.selectTarget({ valid: true, version: 1 }, { valid: false, version: 0 }), 'SLOT_A'));
test('policy A invalid/B valid selects B', () => assert.strictEqual(format.selectTarget({ valid: false, version: 0 }, { valid: true, version: 1 }), 'SLOT_B'));
test('policy both invalid selects programming', () => assert.strictEqual(format.selectTarget({ valid: false, version: 0 }, { valid: false, version: 0 }), 'PROGRAMMING'));
test('policy higher B version selects B', () => assert.strictEqual(format.selectTarget({ valid: true, version: format.parseVersion('1.0.0') }, { valid: true, version: format.parseVersion('1.1.0') }), 'SLOT_B'));
test('policy higher A version selects A', () => assert.strictEqual(format.selectTarget({ valid: true, version: format.parseVersion('1.1.0') }, { valid: true, version: format.parseVersion('1.0.0') }), 'SLOT_A'));
test('policy equal versions selects A', () => assert.strictEqual(format.selectTarget({ valid: true, version: format.parseVersion('1.0.0') }, { valid: true, version: format.parseVersion('1.0.0') }), 'SLOT_A'));

console.log(`PASS ${passed} Stage 2 host tests`);
