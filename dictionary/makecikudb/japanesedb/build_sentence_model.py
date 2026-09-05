"""Build the immutable Japanese Viterbi model consumed by JapaneseSentenceDecoder."""

from __future__ import annotations

import argparse
import math
import json
import hashlib
import tempfile
import shutil
import struct
import urllib.request
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE_DIR = REPO_ROOT / "source" / "mozc_dictionary_oss"
DEFAULT_OUTPUT = REPO_ROOT / "out" / "dict_japanese.dat"
MOZC_RAW = "https://raw.githubusercontent.com/google/mozc/{revision}/src/data/dictionary_oss/{name}"
DICTIONARY_FILES = [f"dictionary{index:02d}.txt" for index in range(10)]
SUPPORT_FILES = ["connection_single_column.txt", "id.def", "README.txt"]
HEADER = struct.Struct("<8sIIIIQQQQ")
TOKEN = struct.Struct("<IHIHHHi")


def download_sources(directory: Path, revision: str) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    marker = directory / "source-manifest.json"
    names = DICTIONARY_FILES + SUPPORT_FILES
    if marker.is_file():
        cached = json.loads(marker.read_text())
        if cached.get("revision") == revision and all(
            (directory / name).is_file() and
            hashlib.sha256((directory / name).read_bytes()).hexdigest() == cached.get("files", {}).get(name)
            for name in names
        ):
            return
    # An interrupted or changed revision cannot reuse a mixture of cached sources.
    with tempfile.TemporaryDirectory(dir=directory.parent) as temporary:
        incoming = Path(temporary)
        digests = {}
        for name in names:
            print(f"Downloading {name} at {revision}...")
            urllib.request.urlretrieve(MOZC_RAW.format(revision=revision, name=name), incoming / name)
            digests[name] = hashlib.sha256((incoming / name).read_bytes()).hexdigest()
        for name in names:
            shutil.copyfile(incoming / name, directory / name)
        marker.write_text(json.dumps({"revision": revision, "files": digests}, indent=2) + "\n")


def read_dictionary(directory: Path) -> list[tuple[str, int, int, int, str]]:
    entries: list[tuple[str, int, int, int, str]] = []
    seen: set[tuple[str, int, int, int, str]] = set()
    for path in (directory / name for name in DICTIONARY_FILES):
        if not path.is_file():
            raise FileNotFoundError(path)
        with path.open("r", encoding="utf-8") as stream:
            for line_number, raw in enumerate(stream, start=1):
                line = raw.rstrip("\r\n")
                if not line or line.startswith("#"):
                    continue
                columns = line.split("\t")
                if len(columns) < 5:
                    raise ValueError(f"{path}:{line_number}: expected at least five tab-separated columns")
                reading, left, right, cost, surface = columns[:5]
                entry = (reading, int(left), int(right), int(cost), surface)
                if entry not in seen:
                    seen.add(entry)
                    entries.append(entry)
    entries.sort(key=lambda item: (item[0], item[4], item[1], item[2], item[3]))
    return entries


def read_connection_costs(directory: Path) -> tuple[int, list[int]]:
    id_path = directory / "id.def"
    connection_path = directory / "connection_single_column.txt"
    if not id_path.is_file() or not connection_path.is_file():
        raise FileNotFoundError("id.def or connection_single_column.txt is missing")
    ids: list[int] = []
    with id_path.open("r", encoding="utf-8") as stream:
        for raw in stream:
            if raw.strip() and not raw.startswith("#"):
                ids.append(int(raw.split(maxsplit=1)[0]))
    size = max(ids) + 1
    costs: list[int] = []
    with connection_path.open("r", encoding="utf-8") as stream:
        for raw in stream:
            value = raw.strip()
            if value and not value.startswith("#"):
                costs.append(max(-32768, min(32767, int(value.split()[0]))))
    if len(costs) == size * size + 1 and costs[0] == size:
        costs = costs[1:]
    if len(costs) != size * size:
        inferred = math.isqrt(len(costs))
        if inferred * inferred != len(costs):
            raise ValueError(f"connection cost count {len(costs)} is not a square matrix")
        print(f"Warning: id.def size {size}; using inferred connection size {inferred}")
        size = inferred
    return size, costs


def build(source_dir: Path, output: Path) -> tuple[int, int]:
    entries = read_dictionary(source_dir)
    connection_size, costs = read_connection_costs(source_dir)
    strings = bytearray()
    string_locations: dict[str, tuple[int, int]] = {}

    def intern(value: str) -> tuple[int, int]:
        existing = string_locations.get(value)
        if existing is not None:
            return existing
        encoded = value.encode("utf-8")
        if len(encoded) > 65535:
            raise ValueError("dictionary string exceeds uint16 length")
        location = (len(strings), len(encoded))
        strings.extend(encoded)
        string_locations[value] = location
        return location

    records = bytearray()
    for reading, left, right, cost, surface in entries:
        if not (0 <= left < connection_size and 0 <= right < connection_size):
            raise ValueError(f"context id outside connection matrix: {left}, {right}")
        reading_offset, reading_length = intern(reading)
        surface_offset, surface_length = intern(surface)
        records.extend(TOKEN.pack(reading_offset, reading_length, surface_offset, surface_length,
                                  left, right, cost))

    token_offset = HEADER.size
    connection_offset = token_offset + len(records)
    connection_bytes = struct.pack(f"<{len(costs)}h", *costs)
    string_offset = connection_offset + len(connection_bytes)
    header = HEADER.pack(b"MSJPDT1\0", 1, len(entries), connection_size, 0,
                         token_offset, connection_offset, string_offset, len(strings))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(header)
        stream.write(records)
        stream.write(connection_bytes)
        stream.write(strings)
    temporary.replace(output)
    return len(entries), connection_size


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--download", action="store_true")
    parser.add_argument("--revision", default=json.loads((REPO_ROOT / "sources-lock.json").read_text())["mozc"]["commit"])
    args = parser.parse_args()
    source_dir = args.source_dir.resolve()
    if args.download:
        download_sources(source_dir, args.revision)
    token_count, connection_size = build(source_dir, args.output.resolve())
    print(f"Built {args.output.resolve()}: {token_count} tokens, {connection_size} context IDs")


if __name__ == "__main__":
    main()
