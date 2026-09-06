"""Generate the machine-specific .clangd for this checkout.

Only .clangd is generated. tests/CMakeLists.txt and tests/CMakePresets.json used to be
generated from copies under config_files/ as well, which meant every source file added to
the real build had to be mirrored into a second copy by hand. It stopped being mirrored,
and running this script would overwrite a working tests/CMakeLists.txt with a stale one
that no longer listed eleven translation units. Both files now resolve Boost and vcpkg from
the environment on their own, so there is nothing left to rewrite.
"""

import os


def normpath(path):
    return path.replace("\\", "/")


def resolve_boost_root(user_home):
    """Mirror the lookup order in tests/CMakeLists.txt so clangd and CMake agree."""
    for name in ("Boost_ROOT", "BOOST_ROOT"):
        value = os.environ.get(name)
        if value:
            return normpath(value)
    return normpath(os.path.join(user_home, "scoop", "apps", "boost", "current"))


cur_file_path = os.path.dirname(os.path.abspath(__file__))
project_root_path = os.path.dirname(os.path.dirname(cur_file_path))

user_home = os.path.expanduser("~")

MetasequoiaImeEngine_root = normpath(project_root_path)
vcpkg_include = normpath(
    os.path.join(
        MetasequoiaImeEngine_root, "tests", "build", "vcpkg_installed", "x64-windows", "include"
    )
)
utfcpp_path = normpath(os.path.join(MetasequoiaImeEngine_root, "utfcpp", "source"))
boost_path = resolve_boost_root(user_home)

dot_clangd_new_lines = [
    f'      "-I{MetasequoiaImeEngine_root}",\n',
    f'      "-I{vcpkg_include}",\n',
    f'      "-I{utfcpp_path}",\n',
    f'      "-I{boost_path}",\n',
]
dot_clangd_file = os.path.join(
    MetasequoiaImeEngine_root, "tests", "scripts", "config_files", ".clangd"
)
dot_clangd_output_file = os.path.join(MetasequoiaImeEngine_root, ".clangd")
with open(dot_clangd_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[6:10] = dot_clangd_new_lines
with open(dot_clangd_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

print(f"wrote {dot_clangd_output_file}")
