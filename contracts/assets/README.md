# Runtime assets contract

`assets.json` is the shared inventory for the desktop runtime bundle. Each entry declares
its stable ID, installation path, role, source and profiles. `user` entries name writable
user files and are deliberately excluded from shipping profiles. Dictionary database
format/schema compatibility remains owned by `contracts/dictionary/`.

- `generate.py` produces `assets.h` for C++ resource lookup and helpcode scheme selection.
- `product.py` reads the inventory for packaging and verifies the exact ZIP member set,
  individual sizes/digests, contract version, dictionary product and source-commit agreement.
- The root builder consumes that inventory; it does not maintain a second filename list.

Run `python3 contracts/assets/generate.py --check` after editing the inventory. CI checks
the generated header and exercises packaging with actual static runtime resources.

Consumers vendor the pinned `contracts/assets/` and `contracts/dictionary/` directories
and use the shared verifier before extraction:

```python
from pathlib import Path
from contracts.assets.product import verify

manifest = verify(Path("engine-assets-desktop.zip"), expected_sha256=product_lock_digest)
```

The expected ZIP digest comes from the platform product lock, not the downloaded manifest.
Verification is read-only and does not extract into application directories. The returned
manifest records the producer commit and Google Pinyin submodule commit. The bundle carries
source/permission notices without changing the underlying data permissions.

`mutable-copy` files are immutable release inputs but writable runtime dictionaries after
installation. `resource` files remain in the resource directory; `user` files remain under
the user's durable data directory. `engine-assets-manifest.json` is versioned separately
from the dictionary database format. Older standalone `dict-*` assets remain supported.
