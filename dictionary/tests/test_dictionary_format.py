import unittest
from dictionary_format import FORMAT, pinyin_table, quanpin_tables

class DictionaryFormatTests(unittest.TestCase):
    def test_shipped_tables_cover_boundary_keys(self):
        tables = quanpin_tables()
        self.assertEqual(FORMAT['formatVersion'], 1)
        self.assertEqual(len(tables), 184)
        for count in (1, 7, 8, 9, 40):
            actual = pinyin_table("'".join(['ni'] * count))
            self.assertEqual(actual, f'tbl_{count}_n' if count <= 7 else 'tbl_others_n')
            self.assertIn(actual, tables)
        self.assertFalse(any(name.startswith('tbl_8_') for name in tables))
        for invalid in ('', "'ni", "ni'", '1ni', '你'):
            self.assertEqual(pinyin_table(invalid), '')
