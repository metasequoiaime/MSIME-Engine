#!/usr/bin/env python3
"""Download the pinned corpus the ngram stage counts, into source/ngram-corpus/.

The corpus is zhwiki rather than C4's Chinese part, and the reason is the measurement rather than the
licence: the harvest eval set the lattice is tuned against (`resources/eval/sentences-v2.tsv` in the
client) is itself harvested from C4, so counting ngrams over C4 would train the decoder on the only
hard benchmark there is. See dictionary/AGENTS.md.

Files, sizes and SHA-1 come from `sources-lock.json`, which records what the dump's own
`dumpstatus.json` published. A file that is already present and matches is left alone, so a partial
download resumes by re-running this.

Wikimedia keeps only the most recent few dumps, so a pin eventually 404s. That is not a silent
failure: the download stops with the URL it could not reach, and re-pinning means putting a newer
dump's list in the lock and rebuilding the tables deliberately.

Usage:
    fetch_corpus.py                 every pinned file, about 3.4 GB
    fetch_corpus.py --limit 1       the first part only, for a local build
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
LOCK = REPO_ROOT / "sources-lock.json"
DESTINATION = REPO_ROOT / "source" / "ngram-corpus"
CHUNK = 1 << 20

# Wikimedia's user-agent policy refuses the default urllib string with a 403 that looks like the dump
# was deleted. Say who is asking and where to complain.
USER_AGENT = "MSIME-Engine-dictionary-build/1.0 (https://github.com/metasequoiaime/MSIME-Engine)"


def digest(path: Path) -> str:
    sha1 = hashlib.sha1()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(CHUNK), b""):
            sha1.update(chunk)
    return sha1.hexdigest()


def fetch(url: str, target: Path, expected: str, size: int) -> None:
    incoming = target.with_suffix(target.suffix + ".incoming")
    try:
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        with urllib.request.urlopen(request) as response, incoming.open("wb") as stream:
            copied = 0
            # Progress only for a human: a build log captured to a file does not want 254 lines of it.
            interactive = sys.stdout.isatty()
            for chunk in iter(lambda: response.read(CHUNK), b""):
                stream.write(chunk)
                copied += len(chunk)
                if interactive:
                    print(f"\r  {target.name}: {copied / 1e6:.0f} / {size / 1e6:.0f} MB", end="", flush=True)
            if not interactive:
                print(f"  {target.name}: {copied / 1e6:.0f} MB")
    except urllib.error.HTTPError as error:
        incoming.unlink(missing_ok=True)
        raise SystemExit(f"{url}: {error}. Wikimedia removes old dumps; re-pin sources-lock.json.")
    if sys.stdout.isatty():
        print()
    actual = digest(incoming)
    if actual != expected:
        incoming.unlink(missing_ok=True)
        raise SystemExit(f"{target.name}: sha1 {actual} does not match the pinned {expected}")
    incoming.replace(target)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--limit", type=int, help="download only the first N pinned files")
    parser.add_argument("--destination", type=Path, default=DESTINATION)
    arguments = parser.parse_args()

    corpus = json.loads(LOCK.read_text(encoding="utf-8"))["ngram_corpus"]
    files = corpus["files"][: arguments.limit] if arguments.limit else corpus["files"]
    arguments.destination.mkdir(parents=True, exist_ok=True)
    print(f"{corpus['source']} {corpus['dump']}, {corpus['license']}: {len(files)} file(s)")
    for entry in files:
        target = arguments.destination / entry["name"]
        if target.is_file() and target.stat().st_size == entry["size"] and digest(target) == entry["sha1"]:
            print(f"  {entry['name']}: already present")
            continue
        fetch(corpus["base_url"] + entry["name"], target, entry["sha1"], entry["size"])
    print(f"corpus ready in {arguments.destination}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
