# Product lock primitives

`product_lock.py` is the shared implementation for the security-sensitive parts of the desktop
product locks: SHA256 verification, dictionary manifest provenance, published checksum parsing,
atomic downloads with retries and annotated/lightweight tag resolution.

The Apple, Linux and Windows repositories keep their own product-specific asset sets, lock schema
and command-line interface. Each repository should vendor this file at its lock helper path and
keep only a thin wrapper around these functions. Its CI must compare the vendored bytes with the
same file from the pinned Engine checkout, for example:

```sh
cmp scripts/product_lock_shared.py vendor/MetasequoiaImeEngine/contracts/product_lock.py
```

The comparison is intentionally byte-for-byte. A platform may add constants and a wrapper beside
the shared file, but changes to the shared implementation belong in Engine first.
