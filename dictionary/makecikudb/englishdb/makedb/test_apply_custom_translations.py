import sqlite3
import tempfile
import unittest
from pathlib import Path

from apply_custom_translations import apply_to_database, parse_translations


class ApplyCustomTranslationsTests(unittest.TestCase):
    def test_classifies_direction_from_source_script(self) -> None:
        entries = parse_translations(
            "# comment\n"
            "华科\tHuazhong University of Science and Technology\n"
            "hust\t华中科技大学\n"
        )
        self.assertEqual(len(entries), 2)
        self.assertTrue(entries[0].chinese_to_english)
        self.assertEqual(entries[0].source, "华科")
        self.assertFalse(entries[1].chinese_to_english)
        self.assertEqual(entries[1].source, "hust")

    def test_later_line_overrides_earlier_line_in_file_order(self) -> None:
        entries = parse_translations("华科\told\n华科\tnew\n")
        self.assertEqual([entry.gloss for entry in entries], ["old", "new"])

    def test_upserts_without_dropping_existing_glosses(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "english.db"
            database = sqlite3.connect(db_path)
            try:
                database.executescript(
                    """
                    CREATE TABLE zh_en_glosses(
                        chinese TEXT COLLATE BINARY PRIMARY KEY,
                        english_gloss TEXT NOT NULL
                    ) WITHOUT ROWID;
                    INSERT INTO zh_en_glosses VALUES('银行','bank');
                    INSERT INTO zh_en_glosses VALUES('华科','Huazhong University of');
                    """
                )
                apply_to_database(
                    database,
                    parse_translations("华科\tHuazhong University of Science and Technology\n"),
                )
                rows = dict(database.execute("SELECT chinese,english_gloss FROM zh_en_glosses"))
            finally:
                database.close()
            self.assertEqual(rows["银行"], "bank")
            self.assertEqual(rows["华科"], "Huazhong University of Science and Technology")


if __name__ == "__main__":
    unittest.main()
