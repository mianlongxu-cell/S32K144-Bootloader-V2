import contextlib
import io
import struct
import unittest
from gateway.boot_status import BootStatus
def payload(astate=5,battempt=2,bstate=4,result=1):
    return struct.pack(">4B2B2xII2B2xII4BII",1,3,1,2,astate,0,0x0C000000,7,bstate,battempt,0x0D000000,9,result,255,255,0,0,battempt)
class BootStatusTests(unittest.TestCase):
    def test_parse_and_describe(self):
        status=BootStatus.parse(payload());self.assertEqual(status.slot_b.attempts,2)
        out=io.StringIO()
        with contextlib.redirect_stdout(out):status.describe()
        rendered=out.getvalue()
        self.assertIn("Slot A: Version 12.0.0.0",rendered)
        self.assertIn("Slot B: Version 13.0.0.0  State TRIAL",rendered)
    def test_rejects_bad_payload(self):
        for data in (b"",payload()[:-1],b"\x02"+payload()[1:]):
            with self.assertRaises(RuntimeError):BootStatus.parse(data)
    def test_invalid_high_version_is_visible_not_promoted(self):
        status=BootStatus.parse(payload(bstate=6,battempt=3,result=4))
        self.assertEqual(status.slot_b.state,6);self.assertEqual(status.active,1)
if __name__=="__main__":unittest.main()
