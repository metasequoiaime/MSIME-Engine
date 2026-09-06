"""Which dictionary inputs carry a redistribution grant, and which do not.

NOTICE.md has always been honest that several inputs have no redistribution grant -- the largest is
`CustomPinyinDictionary`, which is the bulk of `msime.db` -- while also recording that "the unresolved
entries listed in this section do not prevent the product from currently distributing this data".
That is the gap this module closes: the build now leaves those inputs out by default, so what the
three platform installers ship is limited to data the project is entitled to redistribute.

Nothing here decides licensing questions. It records what NOTICE.md's "待解决" section already says
and makes it act on the build. When an upstream grants permission in writing, move its entry from
UNLICENSED to LICENSED here and update NOTICE.md in the same change.

Set MSIME_DICT_INCLUDE_UNLICENSED=1 (or pass --include-unlicensed to build_all.py) to build the
complete dictionary anyway. That is the right choice for local development and for evaluating
recall; it is not a build that should be attached to a release.
"""

from __future__ import annotations

import os

ENV_FLAG = "MSIME_DICT_INCLUDE_UNLICENSED"

# Keyed by the identifier used in NOTICE.md, with the reason it cannot be redistributed today.
UNLICENSED_INPUTS: dict[str, str] = {
    "cn/BaseDictAllV1Part1.txt": "merged from CustomPinyinDictionary, which declares no licence",
    "cn/BaseDictAllV1Part2.txt": "merged from CustomPinyinDictionary, which declares no licence",
    "cn/SingleCharWhitelist.txt": "origin unrecorded; provenance has to be established first",
    "en/oaldpe_words.txt": "extracted from a commercial dictionary",
    "rime-jp_sela": "upstream declares no licence",
}

# What each excluded input falls back to, where a licensed equivalent exists.
FALLBACKS: dict[str, str | None] = {
    # rime-ice is GPL-3.0 and compatible with the frontends. It is a subset of the merged file:
    # excluding the merge costs recall, it does not break the build.
    "cn/BaseDictAllV1Part1.txt": "cn/BaseDictIceV1.txt",
    "cn/BaseDictAllV1Part2.txt": None,
    # Without a provenance record the whitelist cannot be applied, so every single character in the
    # licensed source is accepted instead of the filtered subset.
    "cn/SingleCharWhitelist.txt": None,
    "en/oaldpe_words.txt": None,
    "rime-jp_sela": None,
}


# Only these spellings turn the exclusions off. An allowlist rather than a denylist of falsey
# spellings, because the "on" position of this switch is the one that puts data with no
# redistribution grant into a build: anything unrecognised -- `no`, `off`, `FALSE`, a typo -- has to
# land on the safe side, not enable the risky behaviour by accident.
TRUE_SPELLINGS = frozenset({"1", "true", "yes", "on"})


def include_unlicensed() -> bool:
    """True when the caller has explicitly asked for a complete, non-redistributable build."""
    return os.environ.get(ENV_FLAG, "").strip().lower() in TRUE_SPELLINGS


def is_excluded(identifier: str) -> bool:
    return identifier in UNLICENSED_INPUTS and not include_unlicensed()


def describe_exclusions() -> list[str]:
    """One line per input the current settings leave out, for the build log."""
    if include_unlicensed():
        return []
    lines = []
    for identifier, reason in sorted(UNLICENSED_INPUTS.items()):
        fallback = FALLBACKS.get(identifier)
        replacement = f", using {fallback} instead" if fallback else ""
        lines.append(f"excluded {identifier}: {reason}{replacement}")
    return lines
