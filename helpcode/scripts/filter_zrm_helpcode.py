"""筛选自然码辅助码文件中辅助码恰好为两位字符的记录。"""

from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_INPUT = Path(
    r"C:\Users\SonnyCalcr\Downloads\ZRM_Aux-code-main\ZRM_Aux-code_4.3.txt"
)
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "helpcodes" / "zrm_helpcode.txt"


def filter_helpcode(source: Path, destination: Path) -> tuple[int, int]:
    """保留 `字=两位辅助码` 记录，按完整记录去重并维持原顺序。"""
    seen: set[str] = set()
    result: list[str] = []
    matched_count = 0

    with source.open("r", encoding="utf-8-sig") as input_file:
        for raw_line in input_file:
            line = raw_line.rstrip("\r\n")
            if "=" not in line:
                continue

            _, helpcode = line.split("=", 1)
            if len(helpcode) != 2:
                continue

            matched_count += 1
            if line not in seen:
                seen.add(line)
                result.append(line)

    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.write("\n".join(result))
        if result:
            output_file.write("\n")

    return matched_count, len(result)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="筛选辅助码恰好为两位字符的记录，并按完整记录去重。"
    )
    parser.add_argument("input", nargs="?", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("output", nargs="?", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    matched, written = filter_helpcode(args.input, args.output)
    print(f"读取文件：{args.input}")
    print(f"两位辅助码记录：{matched} 条")
    print(f"去重后写入：{written} 条")
    print(f"输出文件：{args.output}")


if __name__ == "__main__":
    main()
