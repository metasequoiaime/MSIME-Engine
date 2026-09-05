"""Public dictionary format contract; consumed from a pinned Engine revision."""
import json
from pathlib import Path

FORMAT = json.loads(Path(__file__).with_name('format.json').read_text())
QUANPIN = FORMAT['quanpin']


def table_name(syllable_count: int, initial: str) -> str:
    if syllable_count <= 0 or len(initial) != 1 or not 'a' <= initial <= 'z':
        return ''
    bucket = str(syllable_count) if syllable_count <= QUANPIN['maximumNumberedSyllables'] else QUANPIN['overflowBucket']
    return f"{QUANPIN['prefix']}{bucket}_{initial}"


def pinyin_table(key: str) -> str:
    segments = key.split("'")
    return table_name(len(segments), segments[0][0]) if all(segments) else ''


def quanpin_tables():
    return [table_name(count, initial)
            for count in range(1, QUANPIN['maximumNumberedSyllables'] + 2)
            for initial in QUANPIN['initials']]
