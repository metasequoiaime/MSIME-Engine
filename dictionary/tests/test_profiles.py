import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_profile
from profiles.compact import compact_dictionary


class ProfileTests(unittest.TestCase):
    def test_mobile_retains_single_characters_indexes_and_long_phrase_tables(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source.db'
            with sqlite3.connect(source) as database:
                database.executescript("""
                    CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);
                    INSERT INTO tbl_1_n VALUES('ni','n','你',10000),('ni','n','倪',1);
                    CREATE TABLE tbl_others_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);
                    CREATE INDEX long_key ON tbl_others_n(key);
                    INSERT INTO tbl_others_n VALUES('long','l','long phrase',3000),('rare','r','rare phrase',1999);
                    CREATE TABLE wubi86(key TEXT,value TEXT,weight INTEGER);
                """)
            output = root / 'mobile'
            compact_dictionary(source, output / 'msime.db', 2000)
            build_profile.write_manifest(output, 'mobile', ['msime.db', 'msime.db.sha256'], build_profile.digest(source))
            build_profile.verify(output, 'mobile')
            with sqlite3.connect(output / 'msime.db') as database:
                self.assertEqual(database.execute('SELECT count(*) FROM tbl_1_n').fetchone()[0], 2)
                self.assertEqual(database.execute('SELECT value FROM tbl_others_n').fetchall(), [('long phrase',)])
                self.assertEqual(database.execute("SELECT name FROM sqlite_master WHERE type='index'").fetchall(), [('long_key',)])
                self.assertIsNone(database.execute("SELECT name FROM sqlite_master WHERE name='wubi86'").fetchone())
            with self.assertRaises(ValueError):
                build_profile.verify(output, 'desktop')
            with (output / 'msime.db').open('ab') as stream:
                stream.write(b'changed')
            with self.assertRaises(ValueError):
                build_profile.verify(output, 'mobile')

    def test_unknown_format_and_escaped_asset_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = {'manifest_version': 1, 'format_version': 2, 'profile': 'mobile', 'files': {}}
            (root / build_profile.MANIFEST).write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                build_profile.verify(root)
            manifest.update(format_version=1, files={'../msime.db': {}})
            (root / build_profile.MANIFEST).write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                build_profile.verify(root)


if __name__ == '__main__':
    unittest.main()
