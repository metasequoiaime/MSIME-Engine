"""Exercise the public product entry point from outside the source directory."""
import importlib.util
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ConsolidationTests(unittest.TestCase):
    def test_public_mobile_builder_uses_the_current_engine_and_reports_its_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / 'source.db'
            with sqlite3.connect(source) as database:
                database.executescript("""
                    CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);
                    INSERT INTO tbl_1_n VALUES('ni','n','你',10000);
                    CREATE TABLE tbl_2_s(key TEXT,jp TEXT,value TEXT,weight INTEGER);
                    INSERT INTO tbl_2_s VALUES('shui''shan','ss','水杉',3000);
                    INSERT INTO tbl_2_s VALUES('shui''shan','ss','低频词',1);
                """)
            output = directory / 'mobile'
            command = [sys.executable, str(ROOT / 'build_profile.py'), '--profile', 'mobile',
                       '--output', str(output)]
            built = subprocess.run(command + ['--source', str(source)], cwd=directory, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            subprocess.run(command + ['--verify'], cwd=directory, check=True, capture_output=True)
            manifest = json.loads((output / 'dictionary-manifest.json').read_text())
            commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
            self.assertEqual(manifest['source']['repository'], 'metasequoiaime/MSIME-Engine')
            self.assertEqual(manifest['source']['path'], 'dictionary')
            self.assertEqual(manifest['source']['commit'], commit)
            self.assertEqual(manifest['format_contract_commit'], commit)
            with sqlite3.connect(output / 'msime.db') as database:
                self.assertEqual(database.execute('SELECT value FROM tbl_2_s').fetchall(), [('水杉',)])

    def test_public_adapter_exposes_compaction_without_a_nested_engine_checkout(self):
        spec = importlib.util.spec_from_file_location('public_dictionary_builder', ROOT / 'build_profile.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertTrue(callable(module.compact_dictionary))
        self.assertFalse((ROOT / 'dictionary/vendor/MetasequoiaImeEngine').exists())
        self.assertFalse((ROOT / 'dictionary/MetasequoiaImeCustomDict').exists())


if __name__ == '__main__':
    unittest.main()
