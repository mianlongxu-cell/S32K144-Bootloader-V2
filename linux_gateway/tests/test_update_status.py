import contextlib
import io
import struct
import unittest
from unittest.mock import patch
from gateway.update_status import UpdateStatus
from gateway.flasher_v2 import V2UdsFlasher
from gateway.flasher import FlashOptions
from gateway.uds import UdsNegativeResponseError
from test_flasher_v2 import FakeClient, packed

def status(valid=1, fault=0, state=9, result=3):
    return struct.pack(">4B7I",1,valid,fault,0,22,state,0,1,229376,98304,result)

class JournalClient(FakeClient):
    def __init__(self, data):
        super().__init__()
        self.status_data=data
    def read_did(self, did):
        if did==0xF104:
            if self.status_data is None: raise UdsNegativeResponseError(0x22,0x31)
            return self.status_data
        return super().read_did(did)
    def routine_start(self, rid, params=b""):
        if rid==0xFF02:
            assert self.unlocked and params==b"\x00"
            self.status_data=status(state=9,result=11)
        else: super().routine_start(rid,params)

class StatusTests(unittest.TestCase):
    def run_update(self, client, initialize=None):
        output=io.StringIO()
        with patch("gateway.flasher_v2.time.sleep"), contextlib.redirect_stdout(output):
            V2UdsFlasher(client,FlashOptions(),initialize).flash(packed())
        return output.getvalue()
    def test_roundtrip(self):
        s=UpdateStatus.parse(status())
        self.assertEqual((s.transaction_id,s.state,s.active,s.target,s.committed),(22,9,0,1,98304))
    def test_bad_lengths_formats(self):
        for data in (b"",status()[:-1],b"\x02"+status()[1:],status(state=99),status(result=99)):
            with self.assertRaises(RuntimeError): UpdateStatus.parse(data)
    def test_full_restart_from_interruption(self):
        c=JournalClient(status())
        output=self.run_update(c)
        self.assertIn("Previous update interrupted",output)
        self.assertEqual(bytes(c.data),packed())
        self.assertEqual(c.counters[0],1)
    def test_invalid_stops_before_erase(self):
        c=JournalClient(status(valid=0))
        with self.assertRaises(RuntimeError): self.run_update(c)
        self.assertFalse(c.erased)
    def test_storage_fault_stops(self):
        c=JournalClient(status(fault=1))
        with self.assertRaises(RuntimeError): self.run_update(c)
        self.assertFalse(c.erased)
    def test_explicit_commission(self):
        c=JournalClient(status(valid=0))
        self.run_update(c,"A")
        self.assertTrue(c.reset)
    def test_no_reinitialize_valid_record(self):
        c=JournalClient(status())
        with self.assertRaises(RuntimeError): self.run_update(c,"A")
        self.assertFalse(c.erased)
    def test_stage3_compatibility(self):
        c=JournalClient(None)
        self.assertIn("Stage3 ECU",self.run_update(c))
        self.assertTrue(c.reset)
    def test_unsupported_did_not_commissioned(self):
        c=JournalClient(None)
        with self.assertRaises(RuntimeError): self.run_update(c,"A")
        self.assertFalse(c.erased)

if __name__=="__main__": unittest.main()
