import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('probes',Path(__file__).resolve().parents[1]/'tools/output_probe_validation.py')
probes=importlib.util.module_from_spec(spec); spec.loader.exec_module(probes)

class ProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw=b'\x01\x02\x03\xff'*(2560*1440)
        cls.sha=hashlib.sha256(cls.raw).hexdigest()

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.folder=Path(self.temp.name)
        self.rows=[]
        for frame in probes.FRAMES:
            for i,role in enumerate(probes.ROLES):
                x=(i-1)*2560
                self.rows.append(dict(frame=frame,role=role,cpu_sha256='1'*64,cpu_width=512,cpu_height=288,
                    gl_sha256=self.sha,gl_width=2560,gl_height=1440,coloured_pixels=1,written=True,
                    display_ok=True,shader_loaded=True,minimized=False,window_bounds=[x,0,2560,1440],
                    display_bounds=[x,0,2560,1440],display_primary=role=='center',display_id=i+1,
                    window_id=i+1,keyboard_focus=role=='center',input_focus=role=='center',
                    mouse_focus=role=='center',native_foreground=role=='center'))

    def read(self, raw=None):
        (self.folder/'probes.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in self.rows))
        with patch.object(Path,'read_bytes',return_value=self.raw if raw is None else raw):
            return probes.validate(self.folder,physical=True)

    def test_complete_outputs_compare_equal(self):
        first=self.read(); self.assertTrue(first['validated'],first)
        self.assertTrue(probes.compare(first,self.read())['equivalent'])

    def test_truncated_pixels_and_log_rejected(self):
        self.assertFalse(self.read(b'truncated')['validated'])
        (self.folder/'probes.jsonl').write_text('{"frame":')
        result=probes.validate(self.folder,physical=True)
        self.assertFalse(result['validated']); self.assertTrue(result['errors'])

    def test_wrong_monitor_geometry_duplicate_or_missing_output_rejected(self):
        self.rows[0]['display_bounds']=[0,0,7680,1440]
        self.assertFalse(self.read()['validated'])
        self.rows[0]['display_bounds']=[-2560,0,2560,1440]
        self.rows[0]['display_id']=2
        self.assertFalse(self.read()['validated'])
        self.rows.pop()
        self.assertFalse(self.read()['validated'])

    def test_changed_side_cpu_hash_is_not_equivalence(self):
        first=self.read()
        self.rows[0]['cpu_sha256']='2'*64
        result=probes.compare(first,self.read())
        self.assertFalse(result['equivalent'])
        self.assertEqual(result['differences'],[{'frame':1000,'role':'left','field':'cpu_sha256'}])

    def test_wrong_or_missing_physical_focus_rejected(self):
        for key in ('keyboard_focus','input_focus','mouse_focus','native_foreground'):
            for row in self.rows[:3]:
                with self.subTest(field=key,role=row['role']):
                    original=row[key]
                    row[key]=not original
                    self.assertFalse(self.read()['validated'])
                    row[key]=1 if original else 0
                    self.assertFalse(self.read()['validated'])
                    del row[key]
                    self.assertFalse(self.read()['validated'])
                    row[key]=original

    def test_duplicate_invalid_or_changed_window_identity_rejected(self):
        for index,value in ((0,2),(0,0),(0,True),(3,99)):
            with self.subTest(index=index,value=value):
                original=self.rows[index]['window_id']
                self.rows[index]['window_id']=value
                self.assertFalse(self.read()['validated'])
                self.rows[index]['window_id']=original

if __name__=='__main__': unittest.main()
