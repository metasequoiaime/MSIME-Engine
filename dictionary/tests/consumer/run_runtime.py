#!/usr/bin/env python3
"""Verify and consume a produced bundle without touching installed user data."""
import argparse
import hashlib
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from contracts.assets.product import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("consumer", type=Path)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    manifest = verify(args.archive, expected_sha256=args.sha256)
    if manifest["source"]["commit"] != args.source_commit or manifest["source"].get("dirty") is not False:
        raise ValueError("Runtime bundle must come from the expected clean producer commit")
    with tempfile.TemporaryDirectory(prefix="msime-runtime-consumer-") as temporary:
        root = Path(temporary)
        resources = root / "resources"
        # verify() already requires the exact inventory of canonical relative member names.
        with zipfile.ZipFile(args.archive) as archive:
            archive.extractall(resources)
        subprocess.run([str(args.consumer.resolve()), str(resources), str(root / "runtime")], check=True)
        for name, entry in manifest["files"].items():
            with (resources / name).open("rb") as stream:
                hasher = hashlib.sha256()
                for chunk in iter(lambda: stream.read(1 << 20), b""):
                    hasher.update(chunk)
                digest = hasher.hexdigest()
            if digest != entry["sha256"]:
                raise ValueError(f"Runtime consumer modified immutable resource: {name}")
        print(f"Immutable resources unchanged; bundle SHA256 {args.sha256}")


if __name__ == "__main__":
    main()
