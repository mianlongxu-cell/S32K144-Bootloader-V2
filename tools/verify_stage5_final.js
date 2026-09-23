'use strict';
// Read-only verification of the final Stage5 hardware dump and release artifacts.
const fs = require('fs');
const assert = require('assert');
const crypto = require('crypto');
const {crc32} = require('./image_format');
const flash = fs.readFileSync('Application_Build/stage5_final_pflash.bin');
assert.equal(flash.length, 0x80000);
const sha = crypto.createHash('sha256').update(flash).digest('hex');
console.log('PFlash SHA256', sha);
let checked = 0;
for (const line of fs.readFileSync('Application_Build/Stage5/Release/Bootloader.srec','utf8').trim().split(/\r?\n/)) {
  const n = {S1:2,S2:3,S3:4}[line.slice(0,2)];
  if (!n) continue;
  const bytes = Buffer.from(line.slice(2),'hex');
  assert.equal(bytes.length, bytes[0]+1);
  assert.equal(bytes.reduce((a,b)=>a+b,0)&255,255);
  const address = bytes.readUIntBE(1,n);
  const data = bytes.subarray(1+n,-1);
  assert(address+data.length<=0x8000);
  assert(flash.subarray(address,address+data.length).equals(data),`Boot mismatch at ${address.toString(16)}`);
  checked += data.length;
}
console.log('Bootloader SREC matched bytes', checked);
for (const [slot,base,v] of [['a',0x8000,22],['b',0x40000,23]]) {
  const image=fs.readFileSync(`Application_Build/Stage5/APP_${slot.toUpperCase()}/stage5_lpspi_tcrfix_slot_${slot}_v${v}.bin`);
  assert.equal(image.length,0x38000);
  assert(flash.subarray(base,base+image.length).equals(image));
  const h=flash.subarray(base+0x37000,base+0x37040);
  assert.equal(h.readUInt32LE(0),0x42493256);
  assert.equal(crc32(h.subarray(0,60)),h.readUInt32LE(60));
  assert.equal(crc32(flash.subarray(base,base+h.readUInt32LE(8))),h.readUInt32LE(12));
  assert.equal(h.readUInt32LE(16),v*0x1000000);
  console.log('Slot',slot,'exact match; header/payload CRC passed; version',v);
}
const words=(b)=>Array.from({length:b.length/4},(_,i)=>b.readUInt32LE(i*4));
const latest=(a,b)=> {const d=(a.seq-b.seq)>>>0; assert(d!==0&&d!==0x80000000);return d<0x80000000?a:b;};
const journals=[0x78000,0x79000].map(address=>{
  const b=flash.subarray(address,address+128), w=words(b);
  assert(w[0]===0x4a345642&&w[1]===1&&w[31]===0x434f4d54&&w[30]===crc32(b.subarray(0,120)));
  assert(w[6]<=9&&w[12]<=11&&(w[4]<=1||w[4]===0x7fffffff)&&w[5]<=1&&w[4]!==w[5]);
  assert(w[8]<=0x38000&&w[9]<=w[8]&&w[11]<=255&&(w[13]&~1)===0);
  if(w[6]>=1&&w[6]<=8)assert.equal(w[8],0x38000);
  if(w[6]>=5&&w[6]<=8)assert(w[13]===1&&w[9]===w[8]&&w[7]===w[18]&&w[10]===w[17]);
  const r={address,seq:w[2],transaction:w[3],active:w[4],target:w[5],state:w[6],version:w[7],size:w[8],committed:w[9],result:w[12]};
  console.log('Journal valid',JSON.stringify(r));return r;
});
const j=latest(...journals);assert(j.state===0&&j.result===1&&j.target===1&&j.version===0x17000000&&j.committed===0x38000);
console.log('Latest journal IDLE/SUCCESS',JSON.stringify(j));
for(const slot of [0,1]){
  const records=[0,1].map(copy=>{
    const address=0x7a000+slot*0x2000+copy*0x1000,b=flash.subarray(address,address+80),w=words(b);
    assert(w[0]===0x4d355642&&w[1]===1&&w[2]===slot&&w[19]===0x534c4f54&&w[18]===crc32(b.subarray(0,72)));
    assert(w[4]<=6&&w[5]<=3&&w[9]<=8&&w[11]<=6&&(w[12]<=1||w[12]===0x7fffffff)&&(w[15]<=1||w[15]===0x7fffffff)&&w[10]===0&&w[16]===0&&w[17]===0);
    if([2,3,5].includes(w[4]))assert.equal(w[5],0);
    if(w[4]===4)assert(w[5]>=1&&w[5]<=3);
    const r={address,seq:w[8],slot,version:w[3],state:w[4],attempts:w[5],successful:w[6],update:w[7],reset:w[9],result:w[11]};
    console.log('Metadata valid',JSON.stringify(r));return r;
  });
  const m=latest(...records);assert(m.version===(22+slot)*0x1000000&&m.state===5&&m.attempts===0&&m.result===3);
  console.log('Latest metadata CONFIRMED',JSON.stringify(m));
}
console.log('PASS final Stage5 PFlash verification');
