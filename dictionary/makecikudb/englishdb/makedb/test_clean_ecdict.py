import unittest

from clean_ecdict import (
    GlossTerm,
    canonical_reverse_word,
    choose_chinese_gloss,
    extract_gloss_terms,
    finalize_reverse_glosses,
    vocabulary_quality,
)


class CleanEcdictTests(unittest.TestCase):
    def test_extracts_general_terms_and_domain_terms(self) -> None:
        terms = extract_gloss_terms("n. 银行, 堤, 岸\n[医] 库")
        self.assertEqual([term.text for term in terms], ["银行", "堤", "岸", "库"])
        self.assertEqual([term.domain_specific for term in terms], [False, False, False, True])

    def test_extracts_literal_escaped_newlines(self) -> None:
        terms = extract_gloss_terms(r"n. 工具, 器具\nvt. 实现, 执行")
        self.assertEqual([term.text for term in terms], ["工具", "器具", "实现", "执行"])
        self.assertEqual([term.part_of_speech for term in terms], ["n", "n", "vt", "vt"])

    def test_removes_edge_parentheses_but_rejects_internal_explanation(self) -> None:
        terms = extract_gloss_terms("n. （布质）面罩；学位连领帽（表示学位种类）\nv. 使(马,鹰等)戴头罩")
        self.assertEqual([term.text for term in terms], ["面罩", "学位连领帽"])

    def test_candidate_gloss_prefers_general_senses(self) -> None:
        terms = [
            GlossTerm("专业释义", 0, 0, True, "n"),
            GlossTerm("未来", 1, 0, False, "n"),
            GlossTerm("将来", 1, 1, False, "n"),
        ]
        self.assertEqual(choose_chinese_gloss(terms), "未来；将来")

    def test_candidate_gloss_uses_chinese_candidate_weight(self) -> None:
        terms = [
            GlossTerm("官能", 0, 0, False, "n"),
            GlossTerm("职务", 0, 1, False, "n"),
            GlossTerm("功能", 0, 2, False, "n"),
            GlossTerm("函数", 0, 3, False, "n"),
        ]
        weights = {"官能": 10, "职务": 20, "功能": 1_000, "函数": 900}
        self.assertEqual(choose_chinese_gloss(terms, weights), "功能；函数")

    def test_candidate_gloss_removes_redundant_adjectival_form(self) -> None:
        terms = [
            GlossTerm("未来", 0, 0, False, "n"),
            GlossTerm("将来", 0, 1, False, "n"),
            GlossTerm("未来的", 1, 0, False, "a"),
        ]
        weights = {"未来": 100, "将来": 90, "未来的": 1_000}
        self.assertEqual(choose_chinese_gloss(terms, weights), "未来；将来")

    def test_reverse_index_rejects_single_and_domain_terms(self) -> None:
        single = GlossTerm("岸", 0, 0, False, "n")
        domain = GlossTerm("函数", 0, 0, True, "n")
        phrase = GlossTerm("实现", 0, 0, False, "v")
        self.assertFalse(single.reverse_eligible)
        self.assertFalse(domain.reverse_eligible)
        self.assertTrue(phrase.reverse_eligible)

    def test_reverse_index_allows_computing_domain_terms(self) -> None:
        terms = extract_gloss_terms(r"[计] 设置；DOS内部命令:改变环境变量")
        self.assertEqual([term.text for term in terms], ["设置"])
        self.assertTrue(terms[0].reverse_eligible)

    def test_vocabulary_quality_prefers_core_frequent_words(self) -> None:
        common = vocabulary_quality(
            {"collins": "5", "oxford": "1", "tag": "gk cet4", "frq": "500", "bnc": "600"}
        )
        obscure = vocabulary_quality(
            {"collins": "0", "oxford": "0", "tag": "", "frq": "0", "bnc": "0"}
        )
        self.assertGreater(common, obscure)
        self.assertEqual(obscure, 0)

    def test_reverse_gloss_ranking_is_deterministic(self) -> None:
        reverse = {
            "实现": {"realize": 200, "implement": 300, "achieve": 100},
            "未来": {"future": 400},
        }
        self.assertEqual(
            finalize_reverse_glosses(reverse, {"实现", "未来"}),
            {"实现": "implement; realize", "未来": "future"},
        )

    def test_reverse_gloss_omits_low_confidence_second_word(self) -> None:
        reverse = {"未来": {"future": 760_000, "futurity": 40_000}}
        self.assertEqual(finalize_reverse_glosses(reverse, {"未来"}), {"未来": "future"})

    def test_reverse_word_uses_lemma(self) -> None:
        row = {"exchange": "0:bank/1:s"}
        self.assertEqual(canonical_reverse_word(row, "banks", {"bank", "banks"}), "bank")


if __name__ == "__main__":
    unittest.main()
