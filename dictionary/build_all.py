#!/usr/bin/env python3
"""Build every shipping dictionary artifact from the sources in this repository.

The per-database scripts under ``makecikudb/`` stay the source of truth; this module only
orders them and reports what was produced. Each stage runs the existing scripts as separate
processes so their standalone usage keeps working unchanged.

Shipping artifacts, all written to ``out/``:

    msime.db            quanpin, wubi86, quick phrases and the Japanese lexicon
    english.db          English candidates plus the bidirectional ECDICT glosses
    others.db           emoji, kaomoji and symbol catalogs
    dict_japanese.dat   immutable Viterbi model for Japanese sentence decoding

Two stages read data that does not live in this repository. ``--fetch-references`` clones
them at pinned revisions into ``ReferenceProjects/``; without it those stages are skipped
and the build reports them as such rather than failing.

Usage:

    python build_all.py --list
    python build_all.py --fetch-references
    python build_all.py --only quanpin wubi
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
OUT_DIR = REPO_ROOT / "out"
REFERENCE_ROOT = REPO_ROOT.parent / "ReferenceProjects"

# Pinned so a rebuild of the same commit produces the same dictionaries. Bump deliberately.
SOURCES = json.loads((REPO_ROOT / "sources-lock.json").read_text())
REFERENCES = {name: (entry["repository"], entry["commit"]) for name, entry in SOURCES["references"].items()}
MOZC_REVISION = SOURCES["mozc"]["commit"]

# Mozc's README carries the IPAdic / ICOT / Okinawa notices that dict_japanese.dat is derived
# from, so it has to travel with the model. build_sentence_model.py downloads it next to the raw
# data; the japanese-model stage copies it into out/ under a name that says where it came from.
MOZC_NOTICE_SOURCE = REPO_ROOT / "source" / "mozc_dictionary_oss" / "README.txt"
MOZC_NOTICE_NAME = "mozc_dictionary_oss_README.txt"

SHIPPING_ARTIFACTS = (
    "msime.db",
    "english.db",
    "others.db",
    "dict_japanese.dat",
    MOZC_NOTICE_NAME,
)


@dataclass(frozen=True)
class Stage:
    name: str
    description: str
    # Each step is a script path relative to the repository root, plus optional arguments.
    # An argument may contain "{reference}", which expands to --reference-root. The underlying
    # scripts default to <repo parent>/ReferenceProjects, so this only matters when the caller
    # points --reference-root somewhere else.
    steps: tuple[tuple[str, ...], ...]
    produces: tuple[str, ...] = ()
    # Reference checkout this stage cannot run without.
    needs_reference: str | None = None
    # Files that must exist before the stage runs, relative to the repository root.
    needs_paths: tuple[str, ...] = field(default=())
    # Copy the Mozc licence notice into out/ once the stage has produced the model.
    copies_mozc_notice: bool = False


# Order matters. quanpin runs first because create_db_and_table.py creates msime.db itself;
# every later msime.db stage appends its own tables to that file.
#
# Within a stage the step order follows what each directory already documents: the quanpin
# scripts carry numbered docstrings (create, insert, index) while the wubi86 and mix
# directories carry the order in their filenames (01, 02, 03, 04). The quanpin README lists
# create, index, insert instead, which contradicts its own docstrings; the docstring order is
# used here because indexing an empty table first only slows the insert down.
STAGES: tuple[Stage, ...] = (
    Stage(
        name="quanpin",
        description="Quanpin tables in msime.db (tbl_{1..7,others}_{letter})",
        steps=(
            ("makecikudb/quanpindb/makedb/multi_table_has_jp/create_db_and_table.py",),
            ("makecikudb/quanpindb/makedb/multi_table_has_jp/insert_data.py",),
            ("makecikudb/quanpindb/makedb/multi_table_has_jp/create_index_for_db.py",),
        ),
        produces=("msime.db",),
        needs_paths=(
            "cn/SingleCharsAllV1.txt",
            "cn/SingleCharWhitelist.txt",
            "cn/BaseDictAllV1Part1.txt",
            "cn/BaseDictAllV1Part2.txt",
        ),
    ),
    Stage(
        name="wubi",
        description="86 wubi table in msime.db",
        steps=(
            ("makecikudb/wubi86db/makedb/01create_table.py",),
            ("makecikudb/wubi86db/makedb/02create_index.py",),
            ("makecikudb/wubi86db/makedb/03insert_data.py",),
        ),
        produces=("msime.db",),
        needs_paths=("cn/Wubi86.txt",),
    ),
    Stage(
        name="quick-phrases",
        description="Quick phrase table in msime.db",
        steps=(
            ("makecikudb/mixdb/01create_table.py",),
            ("makecikudb/mixdb/02create_index.py",),
            ("makecikudb/mixdb/03insert_data.py",),
            ("makecikudb/mixdb/04verify_db.py",),
        ),
        produces=("msime.db",),
        needs_paths=("mix/quick_phrases.txt",),
    ),
    Stage(
        name="japanese-lexicon",
        description="japanese_lexicon table in msime.db, from the jp_sela Rime dictionary",
        steps=(
            (
                "makecikudb/japanesedb/build_japanese_db.py",
                "--source",
                "{reference}/rime-jp_sela/jp_sela.dict.yaml",
            ),
        ),
        produces=("msime.db",),
        needs_reference="rime-jp_sela",
    ),
    Stage(
        name="english",
        description="english_words table in english.db",
        steps=(
            ("makecikudb/englishdb/makedb/create_db_and_table.py",),
            ("makecikudb/englishdb/makedb/insert_data.py",),
            ("makecikudb/englishdb/makedb/verify_db.py",),
        ),
        produces=("english.db",),
        needs_paths=("en/oaldpe_words.txt", "en/BaseDictIceEn.txt"),
    ),
    Stage(
        name="english-glosses",
        # clean_ecdict.py reads out/msime.db to weight Chinese terms, so quanpin has to have run.
        description="Bidirectional gloss tables in english.db, derived from ECDICT",
        steps=(
            (
                "makecikudb/englishdb/makedb/clean_ecdict.py",
                "--source",
                "{reference}/ECDICT/ecdict.csv",
            ),
        ),
        produces=("english.db",),
        needs_reference="ECDICT",
    ),
    Stage(
        name="custom-translations",
        # Overlays the hand-maintained fixes on top of the ECDICT tables, so it runs last.
        description="Overlay MetasequoiaImeCustomDict translations onto the gloss tables",
        steps=(("makecikudb/englishdb/makedb/apply_custom_translations.py", "--no-app-data"),),
        produces=("english.db",),
        needs_paths=("MetasequoiaImeCustomDict/translations.txt",),
    ),
    Stage(
        name="emoji",
        description="emoji tables in others.db",
        steps=(("makecikudb/emojidb/build_emoji_db.py",),),
        produces=("others.db",),
        needs_paths=("emoji/emoji.txt", "emoji/emoji_catalog.txt", "emoji/emoji_en.txt"),
    ),
    Stage(
        name="kaomoji",
        description="kaomoji table in others.db",
        steps=(("makecikudb/kaomoji/build_kaomoji_db.py",),),
        produces=("others.db",),
        needs_paths=("kaomoji/kaomoji.txt",),
    ),
    Stage(
        name="symbols",
        description="symbol_catalog table in others.db",
        steps=(("makecikudb/symbols/build_symbol_db.py",),),
        produces=("others.db",),
        needs_paths=("symbols/piliapp_symbols.txt",),
    ),
    Stage(
        name="japanese-model",
        description="dict_japanese.dat, the immutable Viterbi model built from Mozc OSS data",
        steps=(
            (
                "makecikudb/japanesedb/build_sentence_model.py",
                "--download",
                "--revision",
                MOZC_REVISION,
            ),
        ),
        produces=("dict_japanese.dat", MOZC_NOTICE_NAME),
        copies_mozc_notice=True,
    ),
)

STAGES_BY_NAME = {stage.name: stage for stage in STAGES}


class BuildError(RuntimeError):
    pass


def fetch_references(reference_root: Path) -> None:
    reference_root.mkdir(parents=True, exist_ok=True)
    for name, (url, revision) in REFERENCES.items():
        destination = reference_root / name
        if not (destination / ".git").is_dir():
            print(f"[reference] cloning {name} from {url}")
            run(["git", "clone", "--filter=blob:none", "--no-checkout", url, str(destination)])
        print(f"[reference] checking out {name} at {revision}")
        run(["git", "-C", str(destination), "fetch", "--depth", "1", "origin", revision])
        run(["git", "-C", str(destination), "checkout", "--force", revision])


def run(command: list[str], cwd: Path | None = None) -> None:
    printable = " ".join(command)
    completed = subprocess.run(command, cwd=cwd)
    if completed.returncode != 0:
        raise BuildError(f"command failed with exit code {completed.returncode}: {printable}")


def missing_inputs(stage: Stage, reference_root: Path) -> list[str]:
    missing = [path for path in stage.needs_paths if not (REPO_ROOT / path).exists()]
    if stage.needs_reference and not (reference_root / stage.needs_reference).is_dir():
        missing.append(f"{reference_root / stage.needs_reference} (use --fetch-references)")
    return missing


def run_stage(stage: Stage, reference_root: Path) -> None:
    for step in stage.steps:
        script = REPO_ROOT / step[0]
        if not script.is_file():
            raise BuildError(f"stage {stage.name}: missing script {script}")
        arguments = [argument.format(reference=reference_root) for argument in step[1:]]
        print(f"  -> {script.relative_to(REPO_ROOT)} {' '.join(arguments)}".rstrip())
        run([sys.executable, str(script), *arguments], cwd=REPO_ROOT)

    if stage.copies_mozc_notice:
        if not MOZC_NOTICE_SOURCE.is_file():
            raise BuildError(f"stage {stage.name}: {MOZC_NOTICE_SOURCE} was not downloaded")
        shutil.copyfile(MOZC_NOTICE_SOURCE, OUT_DIR / MOZC_NOTICE_NAME)
        print(f"  -> copied {MOZC_NOTICE_SOURCE.name} to out/{MOZC_NOTICE_NAME}")


def write_checksums(out_dir: Path) -> Path | None:
    present = [name for name in SHIPPING_ARTIFACTS if (out_dir / name).is_file()]
    if not present:
        return None
    lines = []
    for name in present:
        digest = hashlib.sha256((out_dir / name).read_bytes()).hexdigest()
        lines.append(f"{digest}  {name}")
    target = out_dir / "SHA256SUMS.txt"
    target.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return target


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="print the stages and exit")
    parser.add_argument("--only", nargs="+", metavar="STAGE", help="run only these stages")
    parser.add_argument("--skip", nargs="+", metavar="STAGE", default=[], help="skip these stages")
    parser.add_argument(
        "--fetch-references",
        action="store_true",
        help="clone ECDICT and rime-jp_sela at their pinned revisions before building",
    )
    parser.add_argument(
        "--reference-root",
        type=Path,
        default=REFERENCE_ROOT,
        help=f"where the reference checkouts live (default: {REFERENCE_ROOT})",
    )
    parser.add_argument("--clean", action="store_true", help="delete out/ before building")
    parser.add_argument(
        "--require-all",
        action="store_true",
        help="fail instead of skipping when a stage is missing its inputs",
    )
    args = parser.parse_args()

    unknown = [name for name in (args.only or []) + args.skip if name not in STAGES_BY_NAME]
    if unknown:
        parser.error(f"unknown stage(s): {', '.join(unknown)}; see --list")
    return args


def main() -> int:
    args = parse_args()

    if args.list:
        width = max(len(stage.name) for stage in STAGES)
        for stage in STAGES:
            suffix = f"  [needs {stage.needs_reference}]" if stage.needs_reference else ""
            print(f"{stage.name:<{width}}  {stage.description}{suffix}")
        return 0

    if args.clean and OUT_DIR.exists():
        print(f"[clean] removing {OUT_DIR}")
        shutil.rmtree(OUT_DIR)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    if args.fetch_references:
        fetch_references(args.reference_root)

    selected = [
        stage
        for stage in STAGES
        if (args.only is None or stage.name in args.only) and stage.name not in args.skip
    ]

    skipped: list[tuple[str, list[str]]] = []
    completed: list[tuple[str, float]] = []

    for stage in selected:
        missing = missing_inputs(stage, args.reference_root)
        if missing:
            if args.require_all:
                raise BuildError(f"stage {stage.name} is missing: {', '.join(missing)}")
            print(f"[skip] {stage.name}: missing {', '.join(missing)}")
            skipped.append((stage.name, missing))
            continue

        print(f"[build] {stage.name}: {stage.description}")
        started = time.monotonic()
        run_stage(stage, args.reference_root)
        elapsed = time.monotonic() - started
        completed.append((stage.name, elapsed))
        print(f"[done] {stage.name} in {elapsed:.1f}s")

    checksums = write_checksums(OUT_DIR)

    print("\n=== summary ===")
    for name, elapsed in completed:
        print(f"built    {name} ({elapsed:.1f}s)")
    for name, missing in skipped:
        print(f"skipped  {name}: missing {', '.join(missing)}")
    for name in SHIPPING_ARTIFACTS:
        path = OUT_DIR / name
        if path.is_file():
            print(f"artifact {name} ({path.stat().st_size / 1048576:.1f} MB)")
        else:
            print(f"artifact {name} MISSING")
    if checksums:
        print(f"artifact {checksums.name}")

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BuildError as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
