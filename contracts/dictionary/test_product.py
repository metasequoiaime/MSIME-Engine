import json
from pathlib import Path
import tempfile
import unittest
from product import sha256, verify_product

class ProductTests(unittest.TestCase):
    def test_mobile_integrity_and_compatibility_rejections(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'msime.db').write_bytes(b'fixture')
            (root / 'msime.db.sha256').write_bytes(b'checksum')
            manifest = {'manifest_version': 1, 'format_version': 1, 'profile': 'mobile',
                        'engine_compatibility': {'dictionary_format': 1},
                        'files': {name: {'sha256': sha256(root / name), 'size': (root / name).stat().st_size}
                                  for name in ('msime.db', 'msime.db.sha256')}}
            def write():
                (root / 'dictionary-manifest.json').write_text(json.dumps(manifest))
            write()
            verify_product(root, 'mobile')
            with self.assertRaises(ValueError): verify_product(root, 'desktop')
            manifest['engine_compatibility']['dictionary_format'] = 2
            write()
            with self.assertRaises(ValueError): verify_product(root, 'mobile')
            manifest['engine_compatibility']['dictionary_format'] = 1
            write()
            (root / 'msime.db').write_bytes(b'changed')
            with self.assertRaises(ValueError): verify_product(root, 'mobile')
            with self.assertRaises(ValueError): verify_product(root, 'mobile', ['../msime.db'])
