"""Inspect exact committed source exports without extracting or altering assets."""
import io
from pathlib import Path
import subprocess
import tarfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]

class SourceArchiveTests(unittest.TestCase):
    def check_names(self,names):
        self.assertIn('docs/DISTRIBUTION-AUDIT.md',names)
        self.assertIn('docs/PRODUCT-IDENTITY.md',names)
        self.assertIn('tools/stage_unified.py',names)
        self.assertNotIn('assets/img/boxart.tga',names)
        self.assertFalse(any(n.startswith(('docs/screenshots/','patches/')) for n in names))
        self.assertFalse(any(Path(n).suffix.lower() in ('.sfc','.smc','.bs','.fzpt') for n in names))
        self.assertFalse({'config.ini','fzero-video.ini','keybinds.ini','rom.cfg'} & set(names))

    def test_committed_zip_export(self):
        data=subprocess.check_output(['git','-C',str(ROOT),'archive','--format=zip','HEAD'])
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            self.check_names(archive.namelist())

    def test_committed_tar_export(self):
        data=subprocess.check_output(['git','-C',str(ROOT),'archive','--format=tar','HEAD'])
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            self.check_names(archive.getnames())

if __name__=='__main__':unittest.main()
