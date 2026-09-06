#!/usr/bin/env bash
set -euo pipefail

# Formatting has to be reproducible across contributor machines and CI, so the
# formatter version is pinned here and installed from PyPI instead of being
# taken from whatever the host toolchain happens to ship. Apple, Debian and
# LLVM upstream all disagree about clang-format defaults between releases.
#
# Ported from MSIME-Linux, which has enforced this in CI for a while. The root
# and voice/ .clang-format files are both respected: --style=file picks up the
# nearest one.
clang_format_version=18.1.8

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
venv_root=${METASEQUOIA_CLANG_FORMAT_VENV:-"${TMPDIR:-/tmp}/metasequoia-clang-format-$clang_format_version"}
clang_format="$venv_root/bin/clang-format"

if [[ ! -x "$clang_format" ]]; then
    python3 -m venv "$venv_root"
    "$venv_root/bin/pip" install --quiet "clang-format==$clang_format_version"
fi

mode=${1:---write}
case "$mode" in
    --check) clang_format_arguments=(--dry-run --Werror) ;;
    --write) clang_format_arguments=(-i) ;;
    *)
        echo "Usage: ${BASH_SOURCE[0]} [--check|--write]" >&2
        exit 2
        ;;
esac

cd "$project_root"
# --others --exclude-standard so a newly written file that has not been staged
# yet is still formatted. Without it the script reports a clean tree while
# skipping exactly the file being worked on.
#
# Vendored trees are excluded: googlepinyinime-rev keeps its AOSP formatting,
# utfcpp and voice/third_party are upstream copies, ReferenceProjects is read-only
# reference material, and eng/ is a scratch area.
#
# Generated headers are excluded as well: contracts/{assets,dictionary,webview}/generate.py emit them from JSON, and CI re-runs each generator with --check. Reformatting their output only makes the checked-in file disagree with what the generator produces, which fails that check.
git ls-files --cached --others --exclude-standard '*.cpp' '*.h' '*.hpp' \
    | grep -vE '^(googlepinyinime-rev|utfcpp|ReferenceProjects|eng)/' \
    | grep -v '/third_party/' \
    | grep -vxF -e contracts/assets/assets.h -e contracts/dictionary/format.h -e contracts/webview/schema.h \
    | sort -u \
    | xargs "$clang_format" "${clang_format_arguments[@]}" --style=file
