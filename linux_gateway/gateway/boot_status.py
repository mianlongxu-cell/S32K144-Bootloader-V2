"""Stage5 Bootloader F105 lifecycle status decoder (network byte order)."""
from dataclasses import dataclass
import struct

STATES={0:"EMPTY",1:"DOWNLOADING",2:"VERIFIED",3:"PENDING",4:"TRIAL",5:"CONFIRMED",6:"INVALID",0xFE:"CORRUPT"}
RESULTS=("NONE","TRIAL_STARTED","TRIAL_FAILED","CONFIRMED","ROLLBACK_OCCURRED","NO_CONFIRMED_IMAGE","METADATA_ERROR")
RESETS=("UNKNOWN","POWER_ON","WATCHDOG","SOFTWARE","EXTERNAL","FAULT","LOW_VOLTAGE","CLOCK","DEBUG")
def slot_text(slot:int)->str:return "AB"[slot] if slot in (0,1) else "NONE"
def version_text(value:int)->str:
    return ".".join(str((value>>shift)&255) for shift in (24,16,8,0))

@dataclass(frozen=True)
class SlotLifecycle:
    valid:bool;state:int;attempts:int;version:int;sequence:int

@dataclass(frozen=True)
class BootStatus:
    active:int;last_reset:int;slot_a:SlotLifecycle;slot_b:SlotLifecycle
    result:int;failed_slot:int;rollback_target:int;storage_fault:bool
    failed_version:int;last_attempt_count:int
    @classmethod
    def parse(cls,data:bytes):
        if len(data)!=40:raise RuntimeError("Malformed F105 length")
        values=struct.unpack(">4B2B2xII2B2xII4BII",data)
        abi,mask,active,reset,astate,aattempt,aversion,aseq,bstate,battempt,bversion,bseq,result,failed,rollback,fault,failedver,attempts=values
        if abi!=1 or mask>3 or active not in (0,1,255) or reset>=len(RESETS) or result>=len(RESULTS) or fault>1:
            raise RuntimeError("Unsupported/malformed F105 format")
        if astate not in STATES or bstate not in STATES or failed not in (0,1,255) or rollback not in (0,1,255):
            raise RuntimeError("Malformed F105 fields")
        return cls(active,reset,SlotLifecycle(bool(mask&1),astate,aattempt,aversion,aseq),
                   SlotLifecycle(bool(mask&2),bstate,battempt,bversion,bseq),result,failed,rollback,bool(fault),failedver,attempts)
    def describe(self)->None:
        for name,slot in (("A",self.slot_a),("B",self.slot_b)):
            print(f"Slot {name}: Version {version_text(slot.version)}  State {STATES[slot.state]}  Attempts {slot.attempts}  Metadata {'VALID' if slot.valid else 'UNTRUSTED'}")
        print(f"Active: {slot_text(self.active)}")
        print(f"Last Reset: {RESETS[self.last_reset]}")
        print(f"Last Boot Result: {RESULTS[self.result]}")
        if self.failed_slot!=255:
            print(f"Failed Slot: {slot_text(self.failed_slot)} V{version_text(self.failed_version)}  Attempts {self.last_attempt_count}")
        if self.rollback_target!=255:print(f"Rollback Target: {slot_text(self.rollback_target)}")
        if self.storage_fault:print("Metadata Storage: ERROR (fail-safe)")
