from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from convert_user_dictionary import ConversionError, convert, normalize_reading


class Readings(unittest.TestCase):
    def test_apostrophe_separated_readings_pass_through(self):
        self.assertEqual(normalize_reading("ni'hao", 2), "ni'hao")
        self.assertEqual(normalize_reading("XI'AN", 2), "xi'an")
        # Sogou exports sometimes use a full-width apostrophe.
        self.assertEqual(normalize_reading("ni’hao", 2), "ni'hao")

    def test_a_single_character_needs_no_separator(self):
        self.assertEqual(normalize_reading("hao", 1), "hao")

    def test_an_unseparated_run_is_refused_rather_than_guessed(self):
        # xian is both xi'an and xian. Splitting it here would silently produce wrong entries.
        with self.assertRaises(ConversionError):
            normalize_reading("xian", 2)
        with self.assertRaises(ConversionError):
            normalize_reading("nihao", 2)

    def test_readings_that_are_not_plain_syllables_are_refused(self):
        for reading in ("nǐ'hǎo", "ni3'hao3", "ㄋㄧˇ", "", "ni'"):
            with self.assertRaises(ConversionError, msg=reading):
                normalize_reading(reading, 2)

    def test_syllable_count_must_match_the_word(self):
        with self.assertRaises(ConversionError):
            normalize_reading("ni'hao'ma", 2)


class Rime(unittest.TestCase):
    SOURCE = [
        "---", "name: personal", "...",
        "你好\tni hao\t100",
        "# a comment",
        "",
        "西安\txi'an\t5",
    ]

    def test_entries_after_the_separator_are_converted(self):
        rows, skipped = convert(self.SOURCE, "rime")
        self.assertIn("西安\txi'an\t1", rows)
        # `ni hao` is space separated, which the importer does not accept and this cannot fix.
        self.assertEqual(len(skipped), 1)
        self.assertIn("你好", skipped[0])

    def test_a_file_without_the_separator_is_an_error(self):
        with self.assertRaises(ConversionError):
            convert(["你好\tni'hao"], "rime")

    def test_a_row_with_one_column_is_an_error(self):
        with self.assertRaises(ConversionError):
            convert(["...", "你好"], "rime")


class Csv(unittest.TestCase):
    def test_sogou_style_rows_convert(self):
        rows, skipped = convert(["水杉输入法,shui'shan'shu'ru'fa", "西安,xi'an"], "csv", weight=10)
        self.assertEqual(rows, ["水杉输入法\tshui'shan'shu'ru'fa\t10", "西安\txi'an\t10"])
        self.assertEqual(skipped, [])

    def test_only_the_first_comma_separates(self):
        rows, _ = convert(["你好,ni'hao"], "csv")
        self.assertEqual(rows, ["你好\tni'hao\t1"])

    def test_a_row_with_no_comma_is_an_error(self):
        with self.assertRaises(ConversionError):
            convert(["你好"], "csv")


class Words(unittest.TestCase):
    def test_pinyin_is_generated_when_the_source_has_none(self):
        rows, skipped = convert(["你好", "西安"], "words")
        self.assertEqual(skipped, [])
        self.assertEqual(rows, ["你好\tni'hao\t1", "西安\txi'an\t1"])


class Filtering(unittest.TestCase):
    def test_entries_without_han_characters_are_skipped_with_a_reason(self):
        rows, skipped = convert(["hello,he'llo", "你好,ni'hao"], "csv")
        self.assertEqual(rows, ["你好\tni'hao\t1"])
        self.assertEqual(len(skipped), 1)
        self.assertIn("Han", skipped[0])

    def test_duplicates_are_collapsed(self):
        rows, _ = convert(["你好,ni'hao", "你好,ni'hao"], "csv")
        self.assertEqual(rows, ["你好\tni'hao\t1"])

    def test_the_same_word_with_a_different_reading_is_kept(self):
        rows, _ = convert(["长,chang", "长,zhang"], "csv")
        self.assertEqual(len(rows), 2)

    def test_one_bad_row_does_not_lose_the_rest(self):
        rows, skipped = convert(["你好,nihao", "西安,xi'an"], "csv")
        self.assertEqual(rows, ["西安\txi'an\t1"])
        self.assertEqual(len(skipped), 1)


if __name__ == "__main__":
    unittest.main()
