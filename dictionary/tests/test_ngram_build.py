import json
import os
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
BUILDER = REPO_ROOT / "makecikudb" / "ngramdb" / "build_ngram.py"
sys.path.insert(0, str(BUILDER.parent))
sys.path.insert(0, str(REPO_ROOT))
import build_all  # noqa: E402
from build_ngram import MAGIC, VERSION, corpus_files  # noqa: E402

# The engine maps this file; tests/src/test_ngram_table.cpp reads the same layout from the other side.
HEADER = 16
ENTRY = struct.calcsize("<Q") + struct.calcsize("<f")


def read_table(path: Path):
    data = path.read_bytes()
    magic, version, count = data[:4], *struct.unpack("<II", data[4:12])
    assert magic == MAGIC and version == VERSION, "unexpected table header"
    keys = struct.unpack_from(f"<{count}Q", data, HEADER)
    values = struct.unpack_from(f"<{count}f", data, HEADER + count * 8)
    assert len(data) == HEADER + count * ENTRY, "table length disagrees with its own header"
    return keys, values


def make_dictionary(path: Path, words: list[str]) -> None:
    connection = sqlite3.connect(path)
    connection.execute('create table tbl_2_x ("key" text, "jp" text, "value" text, "weight" integer)')
    connection.executemany("insert into tbl_2_x values ('x', 'x', ?, 1)", [(word,) for word in words])
    connection.commit()
    connection.close()


class CorpusFilesTests(unittest.TestCase):
    def test_a_directory_expands_in_a_stable_order(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "nested").mkdir()
            for name in ("b.txt", "a.txt", "nested/c.txt"):
                (root / name).write_text("x", encoding="utf-8")
            files = corpus_files([root])
            self.assertEqual([path.name for path in files], ["a.txt", "b.txt", "c.txt"])
            self.assertEqual(files, corpus_files([root]), "a second expansion produced a different order")

    def test_a_file_is_passed_through(self):
        with tempfile.TemporaryDirectory() as raw:
            path = Path(raw) / "corpus.txt"
            path.write_text("x", encoding="utf-8")
            self.assertEqual(corpus_files([path]), [path])


class BuildNgramTests(unittest.TestCase):
    """Runs the builder the way the ngram stage does: corpus directory in, packed tables out."""

    def build(self, arguments: list[str]) -> subprocess.CompletedProcess:
        completed = subprocess.run([sys.executable, str(BUILDER), *arguments], capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        return completed

    def test_a_corpus_directory_produces_tables_the_engine_can_map(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            corpus = root / "corpus"
            corpus.mkdir()
            # 开源 社区 repeated often enough to survive --min-count, against a pairing that is not.
            (corpus / "a.txt").write_text("开源社区。" * 40 + "开源产品。", encoding="utf-8")
            dictionary = root / "msime.db"
            make_dictionary(dictionary, ["开源", "社区", "产品"])
            counts = root / "counts.tsv"
            trigram = root / "trigram.bin"
            bigram = root / "bigram.bin"

            self.build(["--dictionary", str(dictionary), "--out", str(trigram), "--counts-out", str(counts),
                        "--order", "3", str(corpus)])
            self.assertTrue(counts.is_file(), "the corpus pass did not write counts for the second table")
            self.build(["--counts-in", str(counts), "--out", str(bigram), "--order", "2"])

            keys, values = read_table(bigram)
            self.assertGreater(len(keys), 0, "the bigram table came out empty")
            self.assertEqual(list(keys), sorted(keys), "the engine binary-searches these keys")
            self.assertTrue(all(abs(value) <= 3.0 for value in values), "a bonus escaped the default clamp")
            self.assertGreater(max(values), 0.0, "a collocation the corpus repeats got no positive bonus")
            read_table(trigram)

    def test_the_second_pass_needs_no_corpus(self):
        # What makes the stage two steps rather than two corpus reads: the expensive pass runs once.
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            corpus = root / "corpus.txt"
            corpus.write_text("开源社区。" * 40, encoding="utf-8")
            dictionary = root / "msime.db"
            make_dictionary(dictionary, ["开源", "社区"])
            counts = root / "counts.tsv"
            self.build(["--dictionary", str(dictionary), "--out", str(root / "t.bin"), "--counts-out", str(counts),
                        "--order", "3", str(corpus)])
            corpus.unlink()
            self.build(["--counts-in", str(counts), "--out", str(root / "b.bin"), "--order", "2"])
            self.assertTrue((root / "b.bin").is_file())


class StageWiringTests(unittest.TestCase):
    def test_the_stage_fetches_its_own_corpus_before_counting(self):
        stage = build_all.STAGES_BY_NAME["ngram"]
        self.assertEqual(stage.steps[0], ("makecikudb/ngramdb/fetch_corpus.py",))
        self.assertEqual(stage.needs_paths, (), "the corpus arrives by download, not from the checkout")

    def test_the_tables_ship(self):
        # dictionary/AGENTS.md lists the six places a new artifact has to join; these are the two this
        # module can see from here. The rest are covered by contracts/dictionary/test_product.py and by
        # contracts/assets/generate.py --check in CI.
        for name in ("bigram.bin", "trigram.bin"):
            self.assertIn(name, build_all.SHIPPING_ARTIFACTS)
            self.assertIn(name, build_all.STAGES_BY_NAME["ngram"].produces)


class CorpusLockTests(unittest.TestCase):
    def test_the_pinned_corpus_is_complete_enough_to_fetch_and_attribute(self):
        corpus = json.loads((REPO_ROOT / "sources-lock.json").read_text(encoding="utf-8"))["ngram_corpus"]
        self.assertEqual(corpus["license"], "CC-BY-SA-4.0")
        self.assertTrue(corpus["base_url"].endswith("/"))
        self.assertTrue(corpus["attribution"])
        self.assertTrue(corpus["files"], "a pin with no files would count an empty corpus")
        self.assertGreaterEqual(corpus["han_characters"], 120_000_000,
                                "the pin has to carry at least build_ngram's default --max-chars")
        for entry in corpus["files"]:
            self.assertEqual(len(entry["sha1"]), 40, entry["name"])
            self.assertGreater(entry["size"], 0, entry["name"])
            self.assertNotIn("/", entry["name"], "a file name has to stay inside the destination")


if __name__ == "__main__":
    unittest.main()
