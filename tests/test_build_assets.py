"""Exercise packaging with actual runtime assets and a minimal dictionary product."""
import json
from pathlib import Path
import tempfile
import subprocess
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT))
import build_assets as assets


class AssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.product = self.directory / 'dictionary'
        self.product.mkdir()
        names = ('msime.db', 'english.db', 'others.db', 'dict_japanese.dat',
                 'mozc_dictionary_oss_README.txt')
        entries = {}
        for name in names:
            data = b'MSJPDT1\0fixture' if name.endswith('.dat') else b'fixture'
            (self.product / name).write_bytes(data)
            entries[name] = {'size': len(data), 'sha256': assets.sha256(data)}
        self.manifest = {'manifest_version': 1, 'format_version': 1, 'profile': 'desktop',
                         'source': {'commit': assets.git('rev-parse', 'HEAD')},
                         'engine_compatibility': {'dictionary_format': 1,
                                                  'japanese_model_magic': 'MSJPDT1'},
                         'files': entries}
        self.save_manifest()

    def save_manifest(self):
        (self.product / 'dictionary-manifest.json').write_text(json.dumps(self.manifest))

    def test_complete_bundle_and_rebuild(self):
        archive = assets.build(self.product, self.directory / 'out')
        manifest = assets.verify(archive)
        self.assertEqual(manifest['source']['commit'], self.manifest['source']['commit'])
        with zipfile.ZipFile(archive) as bundle:
            self.assertNotIn('user_dict.dat', bundle.namelist())
            for name, source in assets.STATIC_FILES.items():
                self.assertEqual(bundle.read(name), (ROOT / source).read_bytes())
        subprocess.run([sys.executable, str(ROOT / 'build_assets.py'), '--verify',
                        '--output', str(archive.parent)], cwd=self.directory, check=True, capture_output=True)
        first = archive.read_bytes()
        assets.build(self.product, archive.parent)
        self.assertEqual(first, archive.read_bytes())

    def test_rejects_wrong_source_and_corrupt_dictionary(self):
        self.manifest['source']['commit'] = '0' * 40
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, 'Rebuild dictionaries'):
            assets.build(self.product, self.directory / 'out')
        (self.product / 'msime.db').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'artifact changed'):
            assets.build(self.product, self.directory / 'out')

    def test_rejects_modified_asset_and_extra_user_data(self):
        archive = assets.build(self.product, self.directory / 'out')
        for target, data in [('dict_pinyin.dat', b'changed'), ('user_dict.dat', b'private')]:
            broken = self.directory / 'broken.zip'
            with zipfile.ZipFile(archive) as source, zipfile.ZipFile(broken, 'w') as output:
                for name in source.namelist():
                    output.writestr(name, data if name == target else source.read(name))
                if target not in source.namelist():
                    output.writestr(target, data)
            with self.assertRaises(ValueError):
                assets.verify(broken)


if __name__ == '__main__':
    unittest.main()
