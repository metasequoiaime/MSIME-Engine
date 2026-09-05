#!/usr/bin/env python3
"""Public dictionary product entry point for the consolidated Engine repository."""
from pathlib import Path
import sys

# The builder's standalone stage scripts retain their dictionary-local imports.
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'dictionary'))
from dictionary.build_profile import compact_dictionary, main, verify  # noqa: E402,F401


if __name__ == '__main__':
    main()
