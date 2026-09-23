#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const format = require('./image_format');

function usage() {
  console.error('Usage: node tools/pack_image.js --slot A|B --input app.bin --output image.bin --version 1.2.3 [--build-id 0x1234] [--flags 0]');
}

function parseArgs(argv) {
  const args = {};
  for (let index = 0; index < argv.length; index += 2) {
    const key = argv[index];
    if (!key.startsWith('--') || index + 1 >= argv.length) {
      throw new Error(`invalid argument near ${key}`);
    }
    args[key.slice(2)] = argv[index + 1];
  }
  for (const required of ['slot', 'input', 'output', 'version']) {
    if (args[required] === undefined) throw new Error(`missing --${required}`);
  }
  return args;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const payload = fs.readFileSync(args.input);
  const softwareVersion = format.parseVersion(args.version);
  const buildId = args['build-id'] === undefined
    ? undefined : format.parseUnsigned(args['build-id'], 'build-id');
  const flags = args.flags === undefined ? 0 : format.parseUnsigned(args.flags, 'flags');
  const packed = format.packImage({
    payload,
    slot: args.slot,
    softwareVersion,
    buildId,
    flags,
  });
  const validation = format.validatePackedImage(packed.image, packed.slot);
  if (validation !== 'VALID') throw new Error(`self-validation failed: ${validation}`);

  fs.mkdirSync(path.dirname(path.resolve(args.output)), { recursive: true });
  fs.writeFileSync(args.output, packed.image);
  const manifestPath = `${args.output}.json`;
  format.writeManifest(manifestPath, {
    format: 'S32K144 Boot Image V2',
    slot: packed.slot.name,
    slot_base: `0x${packed.slot.baseAddress.toString(16).padStart(8, '0')}`,
    slot_size: packed.slot.size,
    header_address: `0x${packed.slot.headerAddress.toString(16).padStart(8, '0')}`,
    vector_address: `0x${packed.slot.vectorAddress.toString(16).padStart(8, '0')}`,
    entry_address: `0x${packed.entryAddress.toString(16).padStart(8, '0')}`,
    initial_msp: `0x${packed.initialMsp.toString(16).padStart(8, '0')}`,
    image_size: packed.imageSize,
    image_crc32: `0x${packed.imageCrc.toString(16).padStart(8, '0')}`,
    software_version: args.version,
    software_version_encoded: `0x${softwareVersion.toString(16).padStart(8, '0')}`,
    build_id: `0x${packed.header.readUInt32LE(20).toString(16).padStart(8, '0')}`,
    header_crc32: `0x${packed.header.readUInt32LE(60).toString(16).padStart(8, '0')}`,
    output: path.resolve(args.output),
    validation,
  });
  console.log(`Packed Slot ${packed.slot.name}: ${args.output}`);
  console.log(`Payload ${packed.imageSize} bytes, CRC32 0x${packed.imageCrc.toString(16).padStart(8, '0')}`);
  console.log(`Header at 0x${packed.slot.headerAddress.toString(16)}, validation ${validation}`);
}

try {
  main();
} catch (error) {
  usage();
  console.error(`ERROR: ${error.message}`);
  process.exitCode = 1;
}
