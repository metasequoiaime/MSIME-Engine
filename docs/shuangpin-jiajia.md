# Pinyin Jiajia profile

Select `GetShuangpinProfile("jiajia")` through `SessionOptions::shuangpin_profile` with
`SchemeType::Shuangpin`. The profile has static lifetime, like the four existing profiles;
the default and unknown-name fallback remain Xiaohe.

Single-letter initials, including written `y` and `w`, retain their keys. The three
compound initials are `zh=v`, `ch=u`, `sh=i`. Finals share these keys:

| Key | Finals | Key | Finals |
| --- | --- | --- | --- |
| a | a | n | iu |
| b | ia, ua | o | o, uo |
| c | uan | p | ou |
| d | ao | q | er, ing |
| e | e | r | en |
| f | an | s | ai |
| g | ang | t | eng |
| h | iang, uang | u | u |
| i | i | v | ui, ü (v) |
| j | ian | w | ei |
| k | iao | x | uai, ue, üe (ve) |
| l | in | y | ong, iong |
| m | ie | z | un |

Vowel-initial syllables use their first letter followed by the final key:
`a=aa, ai=as, an=af, ang=ag, ao=ad, e=ee, ei=ew, en=er, eng=et, er=eq, o=oo, ou=op`.
Written `y/w` syllables use their ordinary initial (`yuan=yc`, `yun=yz`, `weng=wt`).
`ju/qu/xu/yu` retain `u`; `nü/lü` use `nv/lv`, and `nüe/lüe` use `nx/lx`.
The existing decoder also normalizes `jv/qv/xv/yv` to `ju/qu/xu/yu` for this profile.
No semicolon final is assigned.

The existing valid-syllable filter resolves shared finals: `jb/gb` mean `jia/gua`,
`jh/gh` mean `jiang/guang`, `jy/gy` mean `jiong/gong`, and `jx/gx` mean `jue/guai`.
Its historical syllable coverage is unchanged (for example, standalone `yo` is excluded).
The profile does not add an O-prefix or full-pinyin alias table. The shared decoder
naturally also accepts `ai/ao/ei/ou` by combining the first character and its final
key; these existing conversion rules are preserved. For `ai`, `as` is the canonical
encoding, `ai` also decodes, and the O-prefix form `os` is unsupported.

## Provenance

Key assignments were independently expressed in the existing profile structure after
cross-checking the following sources on 2026-09-10; no external conversion implementation
or dictionary was imported.

- [Apple's macOS Tahoe 26 input guide](https://support.apple.com/zh-cn/guide/chinese-input-method/cimf64446397/mac)
  documents the first-vowel-letter rule and `ang=ag`.
- [The original Jiajia forum discussion](https://web.archive.org/web/20140428163004/http://bbs.jjol.cn/showthread.php?t=15979)
  (2013 discussion, 2014-04-28 archive) reports both `as` and historical O-prefix `os`,
  distinguishes the first-letter rule from compatibility input, and provides
  `opeqasig` for `ou'er'ai'shang`. This is a community discussion, not an author-issued specification.
- [Rime's schema 0.03](https://github.com/rime/rime-double-pinyin/blob/01a13287cbd27819be1c34fa1ddc1b3643d5001b/double_pinyin_pyjj.schema.yaml),
  revision `01a13287cbd27819be1c34fa1ddc1b3643d5001b` (repository license: GPL-3.0),
  confirms the keys but also derives compatibility aliases; those are not all required here.
- [Fcitx/libime's independent key table](https://github.com/fcitx/libime/blob/ecd23795ff7ea63a55a1b88fc4767946b999e102/src/libime/pinyin/shuangpindata.h),
  revision `ecd23795ff7ea63a55a1b88fc4767946b999e102`, project version 1.1.16
  (SPDX: LGPL-2.1-or-later), confirms the initial/final assignments. Its general
  O-prefix and full-pinyin compatibility handling is not part of this profile.

The root CMake target `test_shuangpin_profiles` / CTest `shuangpin_profiles` uses explicit
expected syllables, plus a small SQLite fixture to exercise the public session's input,
segmentation, backspace, candidate selection, and existing Microsoft semicolon behavior.
