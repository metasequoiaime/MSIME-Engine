#!/usr/bin/env python3
"""Count word pairs in Chinese text and pack them into the table the lattice reads.

The lattice scores a path as the sum of its words' unigram log probabilities, so nothing in it prefers 配置与权限 over 配置于权限: both spell the same syllables and 于 is the more common character. What is missing is a transition term, and this builds one.

Text is segmented with the same greedy longest match the decoder's vocabulary implies, then each adjacent pair is counted. What gets written is not the conditional probability but the bonus

    log( P(next | previous) / P(next) )

which is zero for an independent pair, positive for a collocation and negative for a pair the corpus avoids. That form is what lets the decoder add the term to its existing score: an absent pair contributes nothing rather than pushing the path off a cliff, so a corpus that has never seen a phrase cannot veto it.

Traditional-Chinese text is left on the floor: the shipped dictionary is simplified, so traditional characters match nothing, break the segmentation chain and drop out. That loses part of a zhwiki dump but corrupts nothing.

Usage:
    build_bigram.py --dictionary out/msime.db --out bigram.bin corpus.xml.bz2
"""

from __future__ import annotations

import argparse
import bz2
import math
import re
import sqlite3
import struct
import sys
from collections import defaultdict
from pathlib import Path

MAGIC = b"MSBG"
VERSION = 1

# One Han character is one syllable, and the dictionary stops at 8-syllable phrases.
MAX_WORD_CHARS = 8

# Stands in for "start of sentence" so the first word of a sentence gets a context too. No real word can collide with it.
START = "\x01"

WIKI_TEXT_RE = re.compile(r"<text\b[^>]*>(.*?)</text>", re.DOTALL)
WIKI_NOISE_RE = re.compile(r"\{\{[^{}]*\}\}|<ref[^>]*>.*?</ref>|<[^>]+>|\[\[[^\]|]*\||[\[\]{}|']", re.DOTALL)


def is_han(ch: str) -> bool:
    return "一" <= ch <= "鿿"


def load_vocabulary(database: Path) -> dict[int, set[str]]:
    """Dictionary values grouped by character count, so segmentation can try the longest first."""
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)
    tables = [row[0] for row in connection.execute("select name from sqlite_master where type='table' and name like 'tbl_%'")]
    by_length: dict[int, set[str]] = defaultdict(set)
    for table in tables:
        for (value,) in connection.execute(f"select value from {table}"):  # noqa: S608 - table names come from sqlite_master
            if not value:
                continue
            length = len(value)
            if 1 <= length <= MAX_WORD_CHARS and all(is_han(ch) for ch in value):
                by_length[length].add(value)
    connection.close()
    return dict(by_length)


def iter_han_runs(inputs: list[Path], max_chars: int):
    """Maximal runs of Han characters, from wiki dumps or plain text, stopping once max_chars have been yielded."""
    produced = 0
    for path in inputs:
        opener = bz2.open if path.suffix == ".bz2" else open
        with opener(path, "rt", encoding="utf-8", errors="ignore") as handle:
            buffer = ""
            for chunk in iter(lambda: handle.read(1 << 20), ""):
                buffer += chunk
                if path.suffix == ".bz2" or "<text" in buffer:
                    pieces = WIKI_TEXT_RE.findall(buffer)
                    cut = buffer.rfind("</text>")
                    buffer = buffer[cut + len("</text>") :] if cut >= 0 else buffer[-(1 << 16) :]
                    body = " ".join(WIKI_NOISE_RE.sub(" ", piece) for piece in pieces)
                else:
                    body, buffer = buffer, ""
                run: list[str] = []
                for ch in body + "\n":
                    if is_han(ch):
                        run.append(ch)
                        continue
                    if len(run) >= 2:
                        text = "".join(run)
                        produced += len(text)
                        yield text
                        if produced >= max_chars:
                            return
                    run = []


def segment(text: str, by_length: dict[int, set[str]], longest: int) -> list[str]:
    words: list[str] = []
    position = 0
    limit = len(text)
    while position < limit:
        for length in range(min(longest, limit - position), 0, -1):
            words_of_length = by_length.get(length)
            if words_of_length is not None and text[position : position + length] in words_of_length:
                words.append(text[position : position + length])
                position += length
                break
        else:
            # A character the dictionary does not know breaks the chain: the words on either side of it were never adjacent.
            words.append("")
            position += 1
    return words


def write_counts(path: Path, unigrams: dict[str, int], bigrams: dict[tuple[str, str], int], characters: int, min_count: int) -> None:
    """Tab-separated counts, so the minutes spent reading a dump are not repeated for every weighting experiment.

    Pairs below min_count are dropped here as well: they cannot reach the packed table from any later run, and keeping them would make the cache larger than the corpus is worth.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        handle.write(f"#characters\t{characters}\n")
        for word, count in unigrams.items():
            handle.write(f"u\t{word}\t{count}\n")
        for (previous, following), count in bigrams.items():
            if count >= min_count:
                handle.write(f"b\t{previous}\t{following}\t{count}\n")


def read_counts(path: Path) -> tuple[defaultdict[str, int], defaultdict[tuple[str, str], int], int]:
    unigrams: defaultdict[str, int] = defaultdict(int)
    bigrams: defaultdict[tuple[str, str], int] = defaultdict(int)
    characters = 0
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            fields = line.rstrip("\n").split("\t")
            if fields[0] == "#characters":
                characters = int(fields[1])
            elif fields[0] == "u":
                unigrams[fields[1]] = int(fields[2])
            elif fields[0] == "b":
                bigrams[(fields[1], fields[2])] = int(fields[3])
    return unigrams, bigrams, characters


def fnv1a64(data: bytes) -> int:
    digest = 0xCBF29CE484222325
    for byte in data:
        digest = ((digest ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inputs", nargs="*", type=Path, help="wiki dump (.bz2) or plain text files; omit with --counts-in")
    parser.add_argument("--dictionary", type=Path, help="path to msime.db; required unless --counts-in is given")
    parser.add_argument("--out", required=True, type=Path, help="packed table to write")
    parser.add_argument("--max-chars", type=int, default=120_000_000, help="stop after this many Han characters")
    parser.add_argument("--min-count", type=int, default=8, help="drop pairs seen fewer times than this")
    parser.add_argument("--shrinkage", type=float, default=25.0, help="pull a pair's bonus toward zero by count/(count+this)")
    parser.add_argument("--clamp", type=float, default=1.5, help="largest bonus, positive or negative, any one pair may carry")
    parser.add_argument("--counts-out", type=Path, help="write the surviving counts here so the corpus pass can be reused")
    parser.add_argument("--counts-in", type=Path, help="read counts from a previous run instead of reading any corpus")
    parser.add_argument("--limit", type=int, default=1_000_000, help="keep at most this many pairs, the most frequent first")
    parser.add_argument("--prune-above", type=int, default=12_000_000, help="drop pairs seen once whenever the table grows past this (0 never prunes)")
    args = parser.parse_args()

    if args.counts_in:
        unigrams, bigrams, characters = read_counts(args.counts_in)
        print(f"counts from {args.counts_in}: {len(unigrams)} words, {len(bigrams)} pairs")
    else:
        if not args.dictionary.is_file():
            print(f"no dictionary at {args.dictionary}", file=sys.stderr)
            return 1
        by_length = load_vocabulary(args.dictionary)
        if not by_length:
            print("empty vocabulary", file=sys.stderr)
            return 1
        longest = max(by_length)
        print(f"vocabulary {sum(len(values) for values in by_length.values())} words, longest {longest} characters")

        unigrams = defaultdict(int)
        bigrams = defaultdict(int)
        characters = 0
        runs = 0
        for text in iter_han_runs(args.inputs, args.max_chars):
            characters += len(text)
            runs += 1
            previous = START
            unigrams[START] += 1
            for word in segment(text, by_length, longest):
                if not word:
                    if previous != START:
                        unigrams[START] += 1
                    previous = START
                    continue
                unigrams[word] += 1
                bigrams[(previous, word)] += 1
                previous = word
            if runs % 200_000 == 0:
                print(f"  {characters} characters, {len(bigrams)} distinct pairs", flush=True)
            # A pair seen once in tens of millions of characters will not survive --min-count anyway, and holding every one of them is what makes this run out of memory on a full dump.
            if args.prune_above and len(bigrams) > args.prune_above:
                bigrams = defaultdict(int, {pair: count for pair, count in bigrams.items() if count > 1})
                print(f"  pruned singletons, {len(bigrams)} pairs left", flush=True)
        if args.counts_out:
            write_counts(args.counts_out, unigrams, bigrams, characters, args.min_count)
            print(f"wrote {args.counts_out}")

    # The start token is a context, never an outcome, so it stays out of the marginal it would otherwise distort.
    total = sum(count for word, count in unigrams.items() if word != START)
    if not total:
        print("corpus produced no words", file=sys.stderr)
        return 1

    entries: list[tuple[int, int, float]] = []
    for (previous, following), count in bigrams.items():
        if count < args.min_count:
            continue
        # How often `previous` occurred at all, not how often it occurred in a pair that survived pruning: taking
        # it from the unigram counts keeps a table built from the cached counts identical to one built in a single
        # pass over the corpus.
        conditional = count / unigrams[previous]
        marginal = unigrams[following] / total
        bonus = math.log(conditional / marginal)
        # A ratio estimated from a handful of occurrences is mostly noise, and left alone it hands a rare word a
        # bonus no common word can answer: 使用 loses to 食用 because the corpus happened to contain a few recipes.
        # Shrinking toward zero by the evidence behind the pair, then capping what any single transition may
        # contribute, keeps the term an adjustment to the unigram ranking rather than a replacement for it.
        bonus *= count / (count + args.shrinkage)
        bonus = max(-args.clamp, min(args.clamp, bonus))
        key = fnv1a64(previous.encode("utf-8") + b"\x00" + following.encode("utf-8"))
        entries.append((count, key, bonus))

    entries.sort(key=lambda item: item[0], reverse=True)
    kept = entries[: args.limit] if args.limit else entries
    packed = sorted((key, value) for _, key, value in kept)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("wb") as handle:
        handle.write(MAGIC)
        handle.write(struct.pack("<III", VERSION, len(packed), 0))
        handle.write(struct.pack(f"<{len(packed)}Q", *(key for key, _ in packed)))
        handle.write(struct.pack(f"<{len(packed)}f", *(value for _, value in packed)))

    print(f"{characters} characters, {total} words, {len(bigrams)} distinct pairs, {len(packed)} kept")
    print(f"wrote {args.out} ({args.out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
