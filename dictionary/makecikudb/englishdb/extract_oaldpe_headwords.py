"""Extract an English-word whitelist from an OALDPE MDict file.

The .mdx itself is a commercial dictionary and is not distributed with this
repository; supply your own copy as the command-line argument. The build does
not need it -- only the already-extracted en/oaldpe_words.txt is consumed.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import types


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_OUTPUT_PATH = REPOSITORY_ROOT / "en" / "oaldpe_words.txt"


def load_mdx_class():
    """Import readmdict while allowing zlib-only MDict 2.0 files on Windows."""
    try:
        import lzo  # type: ignore[import-not-found]  # noqa: F401
    except ModuleNotFoundError:
        # readmdict 0.1.1 refuses to import without python-lzo, even though MDict
        # 2.0 key blocks normally use zlib. Provide a clear failure only if an
        # actual LZO-compressed block is encountered.
        lzo_stub = types.ModuleType("lzo")

        def unsupported_lzo(_data: bytes) -> bytes:
            raise RuntimeError(
                "This MDX contains an LZO-compressed block. Install python-lzo "
                "and run the command again."
            )

        lzo_stub.decompress = unsupported_lzo  # type: ignore[attr-defined]
        sys.modules["lzo"] = lzo_stub

    try:
        from readmdict import MDX
    except ImportError as error:
        raise RuntimeError(
            "Missing dependency 'readmdict'. Run: python -m pip install -r requirements.txt"
        ) from error
    return MDX


def decode_headword(raw_key: bytes) -> str:
    return raw_key.decode("utf-8", errors="strict").strip()


def extract_words(mdx_path: Path, include_all_headwords: bool) -> tuple[list[str], int]:
    MDX = load_mdx_class()
    dictionary = MDX(str(mdx_path))

    if include_all_headwords:
        # Preserve spelling and punctuation in this diagnostic form, while
        # removing exact duplicate keys.
        headwords = {
            headword
            for raw_key in dictionary.keys()
            if (headword := decode_headword(raw_key))
        }
        words = sorted(headwords, key=lambda value: (value.casefold(), value))
    else:
        words_set: set[str] = set()
        for raw_key, raw_record in dictionary.items():
            headword = decode_headword(raw_key)
            if not headword.isascii() or not headword.isalpha():
                continue

            if b"oald-entry-root" in raw_record:
                words_set.add(headword.lower())
                continue

            stripped_record = raw_record.lstrip()
            if not stripped_record.startswith(b"@@@LINK="):
                continue

            link_target = stripped_record[len(b"@@@LINK=") :].decode(
                "utf-8", errors="strict"
            ).strip()
            # Keep inflected forms such as "aardvarks -> aardvark", but reject
            # concatenated search aliases such as
            # "aaroncopland -> aaron-copland".
            if link_target.isascii() and link_target.isalpha():
                words_set.add(headword.lower())
        words = sorted(words_set)

    return words, len(dictionary)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract OALDPE headwords. By default only lowercase ASCII single "
            "words are written, suitable for filtering google_count_1_w.txt."
        )
    )
    parser.add_argument("mdx", type=Path, help="path to oaldpe.mdx")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT_PATH,
        help=f"output text file (default: {DEFAULT_OUTPUT_PATH})",
    )
    parser.add_argument(
        "--all-headwords",
        action="store_true",
        help="retain phrases, punctuation and original casing instead of producing a word whitelist",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    mdx_path = args.mdx.resolve()
    output_path = args.output.resolve()
    if not mdx_path.is_file():
        raise FileNotFoundError(mdx_path)
    if mdx_path.suffix.lower() != ".mdx":
        raise ValueError(f"Expected an .mdx file: {mdx_path}")

    words, source_count = extract_words(mdx_path, args.all_headwords)
    if not words:
        raise RuntimeError(f"No matching headwords found in {mdx_path}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("".join(f"{word}\n" for word in words), encoding="utf-8")

    print(f"MDX index keys: {source_count}")
    print(f"Unique exported headwords: {len(words)}")
    print(f"Output: {output_path}")


if __name__ == "__main__":
    main()
