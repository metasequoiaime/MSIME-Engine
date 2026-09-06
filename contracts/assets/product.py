"""Shared asset inventory for builders and pinned consumers; standard library only."""
import json
import hashlib
import tempfile
import zipfile
import importlib.util
import re
from pathlib import Path, PurePosixPath

_dictionary_spec = importlib.util.spec_from_file_location(
    "msime_assets_dictionary_contract", Path(__file__).parents[1] / "dictionary/product.py")
_dictionary = importlib.util.module_from_spec(_dictionary_spec)
_dictionary_spec.loader.exec_module(_dictionary)
verify_product = _dictionary.verify_product

CONTRACT = json.loads(Path(__file__).with_name('assets.json').read_text(encoding='utf-8'))
# Fail before generating code or touching an archive if the inventory itself is ambiguous.
_ids, _paths = set(), set()
for _entry in CONTRACT['assets']:
    _name = _entry['path']
    if (not re.fullmatch(r'[a-z][a-z0-9_]*', _entry['id']) or _entry['id'] in _ids or
            _name.casefold() in _paths or PurePosixPath(_name).is_absolute() or
            '..' in PurePosixPath(_name).parts or '\\' in _name or
            str(PurePosixPath(_name)) != _name):
        raise ValueError('Invalid or duplicate assets contract entry')
    _ids.add(_entry['id'])
    _paths.add(_name.casefold())
VERSION = CONTRACT['contract_version']
ARCHIVE = CONTRACT['archive']
MANIFEST = CONTRACT['manifest']


def assets(profile='desktop'):
    if profile != 'desktop':
        raise ValueError(f'Unsupported assets profile: {profile}')
    return {entry['path']: entry for entry in CONTRACT['assets'] if profile in entry['profiles']}


STATIC_FILES = {name: entry['source'] for name, entry in assets().items()
                if entry['source'] != 'dictionary'}
DICTIONARY_FILES = frozenset(assets()) - STATIC_FILES.keys()
HELPCODES = {entry['schema']: entry['path'] for entry in CONTRACT['assets'] if 'schema' in entry}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def verify(archive: Path, expected_sha256: str | None = None) -> dict:
    """Verify the exact payload and optionally its digest pinned by the consuming product."""
    if expected_sha256 is not None:
        digest = hashlib.sha256()
        with Path(archive).open('rb') as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b''):
                digest.update(chunk)
        if digest.hexdigest() != expected_sha256:
            raise ValueError('Engine assets archive does not match pinned digest')
    with zipfile.ZipFile(archive) as bundle:
        names = bundle.namelist()
        manifest = json.loads(bundle.read(MANIFEST))
        expected = set(assets())
        if (manifest.get('manifest_version') != 1 or manifest.get('assets_contract_version') != VERSION or manifest.get('profile') != 'desktop'
                or set(manifest.get('files', {})) != expected
                or len(names) != len(set(names))
                or set(names) != expected | {MANIFEST}):
            raise ValueError('Unexpected engine assets manifest or archive contents')
        if manifest.get('source', {}).get('repository') != 'metasequoiaime/MSIME-Engine':
            raise ValueError('Unexpected Engine assets source repository')
        for name, entry in manifest['files'].items():
            data = bundle.read(name)
            if len(data) != entry['size'] or sha256(data) != entry['sha256']:
                raise ValueError(f'Engine asset changed: {name}')
        # Reuse the dictionary format contract, including model magic and profile checks.
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name in expected - set(STATIC_FILES):
                (directory / name).write_bytes(bundle.read(name))
            product = verify_product(directory, 'desktop')
        if product['source']['commit'] != manifest['source']['commit']:
            raise ValueError('Dictionary and runtime assets must have the same source commit')
        return manifest

