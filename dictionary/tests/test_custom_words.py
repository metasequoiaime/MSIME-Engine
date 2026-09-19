import sqlite3
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))
sys.path.insert(0, str(REPO_ROOT / "makecikudb" / "quanpindb" / "makedb" / "multi_table_has_jp"))
from dictionary_format import pinyin_table  # noqa: E402
from insert_custom_words import (CustomWord, apply_custom_words,  # noqa: E402
                                 parse_custom_words)

WORDS_PATH = REPO_ROOT / "custom" / "words.txt"


def table_of(key: str) -> str:
    return pinyin_table(key)


class ParseCustomWordsTests(unittest.TestCase):
    def test_reads_word_pinyin_and_weight(self):
        entries = parse_custom_words("# comment\n\n未来可期\twei'lai'ke'qi\t5000\n")
        self.assertEqual(len(entries), 1)
        entry = entries[0]
        self.assertEqual((entry.value, entry.key, entry.weight), ("未来可期", "wei'lai'ke'qi", 5000))
        self.assertEqual(entry.jp, "wlkq")
        self.assertEqual(entry.table, "tbl_4_w")

    def test_a_repeated_entry_keeps_the_higher_weight(self):
        entries = parse_custom_words("堪堪\tkan'kan\t1\n堪堪\tkan'kan\t400\n")
        self.assertEqual([entry.weight for entry in entries], [400])

    def test_a_malformed_line_fails_the_stage(self):
        for line in (
            "未来可期\twei'lai'ke'qi",  # missing weight
            "未来可期\twei'lai'ke'qi\t5000\textra",
            "\twei'lai'ke'qi\t5000",  # empty word
            "未来可期\tWei'lai'ke'qi\t5000",  # not lowercase quanpin
            "未来可期\twei lai ke qi\t5000",
            "未来可期\twei'lai'ke'qi\tmany",
            "未来可期\twei'lai'ke'qi\t0",
        ):
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_custom_words(line + "\n")

    def test_the_shipped_file_parses(self):
        entries = parse_custom_words(WORDS_PATH.read_text(encoding="utf-8"))
        self.assertGreaterEqual(len(entries), 39)
        values = {entry.value for entry in entries}
        # The two entries this stage was wired in for: 未来可期 was absent from the general
        # dictionary altogether, and 扛把子 shipped at a weight the sentence path never reached.
        self.assertIn("未来可期", values)
        self.assertIn("扛把子", values)


class ApplyCustomWordsTests(unittest.TestCase):
    def setUp(self):
        self.connection = sqlite3.connect(":memory:")
        for table in ("tbl_2_k", "tbl_3_k", "tbl_4_w"):
            self.connection.execute(
                f'create table {table} ("key" text, "jp" text, "value" text, "weight" integer default 0)'
            )

    def tearDown(self):
        self.connection.close()

    def rows(self, table: str) -> list[tuple]:
        return list(self.connection.execute(f"select key, jp, value, weight from {table} order by value"))

    def test_inserts_an_entry_the_dictionary_does_not_have(self):
        counts = apply_custom_words(self.connection, [CustomWord("未来可期", "wei'lai'ke'qi", 5000)])
        self.assertEqual(counts, {"inserted": 1, "promoted": 0, "unchanged": 0})
        self.assertEqual(self.rows("tbl_4_w"), [("wei'lai'ke'qi", "wlkq", "未来可期", 5000)])

    def test_raises_the_weight_of_an_entry_that_is_already_there(self):
        self.connection.execute(
            "insert into tbl_3_k (key, jp, value, weight) values ('kang''ba''zi', 'kbz', '扛把子', 100)"
        )
        counts = apply_custom_words(self.connection, [CustomWord("扛把子", "kang'ba'zi", 10000)])
        self.assertEqual(counts, {"inserted": 0, "promoted": 1, "unchanged": 0})
        self.assertEqual(self.rows("tbl_3_k"), [("kang'ba'zi", "kbz", "扛把子", 10000)])

    def test_never_lowers_a_weight_the_general_dictionary_earned(self):
        # Most of the hand-maintained entries carry a placeholder weight of 1. Taking the file
        # literally would drop a word like 属于是 to the bottom of its own page.
        self.connection.execute(
            "insert into tbl_2_k (key, jp, value, weight) values ('ke''qi', 'kq', '客气', 75460)"
        )
        counts = apply_custom_words(self.connection, [CustomWord("客气", "ke'qi", 1)])
        self.assertEqual(counts, {"inserted": 0, "promoted": 0, "unchanged": 1})
        self.assertEqual(self.rows("tbl_2_k"), [("ke'qi", "kq", "客气", 75460)])

    def test_running_the_stage_twice_changes_nothing(self):
        entries = [CustomWord("未来可期", "wei'lai'ke'qi", 5000), CustomWord("扛把子", "kang'ba'zi", 10000)]
        apply_custom_words(self.connection, entries)
        before = self.rows("tbl_4_w") + self.rows("tbl_3_k")
        counts = apply_custom_words(self.connection, entries)
        self.assertEqual(counts, {"inserted": 0, "promoted": 0, "unchanged": 2})
        self.assertEqual(self.rows("tbl_4_w") + self.rows("tbl_3_k"), before)


if __name__ == "__main__":
    unittest.main()
