# Dictionary format v1

`format.json` is the source of truth for Quanpin table naming and the data format version.
`generate.py` emits the C++ header; `format.py` reads the same JSON for dictionary builders.
The Engine query and journal replay use this header, and Server settings writes through the
Engine's public `quanpin::build_table_name`. Dict consumes the Python API from a pinned Engine
revision, including table enumeration for creation and indexing.

The numbered buckets stop at seven syllables. Eight and more use `tbl_others_{initial}`.
The shipped table set uses the listed initial letters; query construction may also receive
an unmatched lowercase initial and return an empty result from the dictionary. Malformed
or absent initial characters cannot become SQL identifiers.

Change the format version when changing the on-disk layout. Keep compatibility with the
format range declared by dictionary product manifests. Run the Engine input-session tests:
they create seven/eight/nine-syllable words, query them and replay their journal into a fresh
database. CI also rejects stale generated headers.
