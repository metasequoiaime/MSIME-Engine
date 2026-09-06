#!/usr/bin/env python3
"""Package the desktop dictionary product and shared runtime assets (stdlib only)."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile

from dictionary.dictionary_format import verify_product

from contracts.assets.product import ARCHIVE, MANIFEST, VERSION, STATIC_FILES, verify

ROOT = Path(__file__).resolve().parent


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git(*args: str) -> str:
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()


def build(dictionary_dir: Path, output: Path) -> Path:
    product = verify_product(dictionary_dir, 'desktop')
    commit = git('rev-parse', 'HEAD')
    if product['source']['commit'] != commit:
        raise ValueError('Rebuild dictionaries from this Engine commit before packaging assets')
    files = {name: dictionary_dir / name for name in product['files']}
    files['dictionary-manifest.json'] = dictionary_dir / 'dictionary-manifest.json'
    files.update({name: ROOT / source for name, source in STATIC_FILES.items()})
    manifest = {
        'manifest_version': 1, 'assets_contract_version': VERSION, 'profile': 'desktop',
        'source': {'repository': 'metasequoiaime/MSIME-Engine', 'commit': commit,
                   'dirty': bool(product['source'].get('dirty')) or bool(
                       git('status', '--porcelain', '--untracked-files=no'))},
        'googlepinyinime_commit': git('-C', 'googlepinyinime-rev', 'rev-parse', 'HEAD'),
        'files': {},
    }
    output.mkdir(parents=True, exist_ok=True)
    # Build in a fresh temporary archive: obsolete files cannot leak into a new release.
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        archive = Path(temporary) / ARCHIVE
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as bundle:
            for name, path in sorted(files.items()):
                data = path.read_bytes()
                if not data:
                    raise ValueError(f'Empty engine asset: {path}')
                manifest['files'][name] = {'size': len(data), 'sha256': sha256(data),
                                           'source': STATIC_FILES.get(name, f'dictionary/out/desktop/{name}')}
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                bundle.writestr(info, data)
            info = zipfile.ZipInfo(MANIFEST, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            bundle.writestr(info, json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
        verify(archive)
        archive.replace(output / ARCHIVE)
    (output / f'{ARCHIVE}.sha256').write_text(
        f'{sha256((output / ARCHIVE).read_bytes())}  {ARCHIVE}\n', encoding='ascii')
    return output / ARCHIVE


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dictionary-dir', type=Path, default=ROOT / 'dictionary/out/desktop')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/assets')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        archive = args.output / ARCHIVE
        expected = f'{sha256(archive.read_bytes())}  {ARCHIVE}\n'
        if (args.output / f'{ARCHIVE}.sha256').read_text(encoding='ascii') != expected:
            raise ValueError('Engine assets archive checksum mismatch')
        verify(archive)
    else:
        print(build(args.dictionary_dir, args.output))


if __name__ == '__main__':
    main()
