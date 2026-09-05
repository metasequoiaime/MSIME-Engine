"""根据 helpcode.txt 中已有的汉字筛选 small 版自然码辅助码表。"""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_INPUT = ROOT / "helpcodes" / "zrm_helpcode_big.txt"
DEFAULT_REFERENCE = ROOT / "helpcodes" / "helpcode.txt"
DEFAULT_OUTPUT = ROOT / "helpcodes" / "zrm_helpcode.txt"


def read_keys(path: Path) -> set[str]:
    """读取等号左侧的全部键（汉字或词条）。"""
    keys: set[str] = set()
    with path.open("r", encoding="utf-8-sig") as file:
        for raw_line in file:
            line = raw_line.rstrip("\r\n")
            if "=" in line:
                key, _ = line.split("=", 1)
                keys.add(key)
    return keys


def create_small_table(source: Path, reference: Path, output: Path) -> tuple[int, int]:
    """保留 source 中键存在于 reference 的所有记录，按完整行去重。"""
    wanted_keys = read_keys(reference)
    seen: set[str] = set()
    result: list[str] = []

    with source.open("r", encoding="utf-8-sig") as file:
        for raw_line in file:
            line = raw_line.rstrip("\r\n")
            if "=" not in line:
                continue
            key, _ = line.split("=", 1)
            if key in wanted_keys and line not in seen:
                seen.add(line)
                result.append(line)

    with output.open("w", encoding="utf-8", newline="\n") as file:
        file.write("\n".join(result))
        if result:
            file.write("\n")

    return len(wanted_keys), len(result)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="按 helpcode.txt 已有汉字筛选 small 版辅助码表。"
    )
    parser.add_argument("input", nargs="?", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("reference", nargs="?", type=Path, default=DEFAULT_REFERENCE)
    parser.add_argument("output", nargs="?", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    key_count, line_count = create_small_table(
        args.input, args.reference, args.output
    )
    print(f"参考汉字：{key_count} 个")
    print(f"写入记录：{line_count} 条")
    print(f"输出文件：{args.output}")


if __name__ == "__main__":
    main()
