#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const format = require('./image_format');

function parseArgs(argv) {
  const args = {};
  for (let index = 0; index < argv.length; index += 2) {
    if (!argv[index].startsWith('--') || index + 1 >= argv.length) {
      throw new Error('arguments must be --name value pairs');
    }
    args[argv[index].slice(2)] = argv[index + 1];
  }
  for (const required of ['slot', 'input', 'output', 'mode']) {
    if (!args[required]) throw new Error(`missing --${required}`);
  }
  return args;
}

function refreshHeaderCrc(image, headerOffset) {
  const value = format.crc32(image.subarray(
    headerOffset, headerOffset + format.HEADER_CRC_LENGTH));
  image.writeUInt32LE(value, headerOffset + 60);
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const slot = format.slotFor(args.slot);
  const image = Buffer.from(fs.readFileSync(args.input));
  const headerOffset = slot.headerAddress - slot.baseAddress;
  const header = format.readHeader(image, slot);

  switch (args.mode) {
    case 'magic':
      image.writeUInt32LE((header.magic ^ 1) >>> 0, headerOffset);
      break;
    case 'header-crc':
      image.writeUInt32LE((header.headerCrc32 ^ 1) >>> 0, headerOffset + 60);
      break;
    case 'image-crc':
      image[8] ^= 1;
      break;
    case 'vector':
      image.writeUInt32LE((slot.vectorAddress + 0x400) >>> 0, headerOffset + 24);
      refreshHeaderCrc(image, headerOffset);
      break;
    case 'reset-handler': {
      image.writeUInt32LE(0xFFFFFFFF, 4);
      const payloadCrc = format.crc32(image.subarray(0, header.imageSize));
      image.writeUInt32LE(payloadCrc, headerOffset + 12);
      image.writeUInt32LE(0xFFFFFFFF, headerOffset + 28);
      refreshHeaderCrc(image, headerOffset);
      break;
    }
    default:
      throw new Error('mode must be magic, header-crc, image-crc, vector, or reset-handler');
  }

  fs.mkdirSync(path.dirname(path.resolve(args.output)), { recursive: true });
  fs.writeFileSync(args.output, image);
  console.log(`Created ${args.mode} test image: ${args.output}`);
  console.log(`Expected validation failure: ${format.validatePackedImage(image, slot)}`);
}

try {
  main();
} catch (error) {
  console.error(`ERROR: ${error.message}`);
  process.exitCode = 1;
}
