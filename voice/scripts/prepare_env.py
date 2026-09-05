import os


def normpath(path):
    return path.replace("\\", "/")


local_app_data_dir = os.environ.get("LOCALAPPDATA")
home_dir = os.path.expanduser("~")
cur_file_path = os.path.dirname(os.path.abspath(__file__))
project_root_path = os.path.dirname(cur_file_path)
user_home = os.path.expanduser("~")

Project_root_path = normpath(project_root_path)
Project_src_path = normpath(os.path.join(Project_root_path, "src"))

vcpkg_include_path = normpath(
    os.path.join(
        Project_root_path,
        "build",
        "vcpkg_installed",
        "x64-windows-ort-static",
        "include",
    )
)
miniaudio_include_path = normpath(
    os.path.join(Project_root_path, "third_party", "miniaudio")
)
whisper_include_path = normpath(
    os.path.join(Project_root_path, "third_party", "whisper.cpp", "include")
)
ggml_include_path = normpath(
    os.path.join(Project_root_path, "third_party", "whisper.cpp", "ggml", "include")
)

utfcpp_path = normpath(os.path.join(Project_root_path, "utfcpp", "source"))
boost_path = normpath(os.path.join(user_home, "scoop", "apps", "boost", "current"))
vcpkg_root_path = normpath(os.path.join(user_home, "scoop", "apps", "vcpkg", "current"))
cmake_toolchain_file_path = normpath(
    os.path.join(
        user_home,
        "scoop",
        "apps",
        "vcpkg",
        "current",
        "scripts",
        "buildsystems",
        "vcpkg.cmake",
    )
)


#
# project_root/.clangd
# line 8, 9, 10, 11, 12
#
dot_clangd_file = os.path.join(Project_root_path, "scripts", "config_files", ".clangd")
dot_clangd_output_file = os.path.join(Project_root_path, ".clangd")
with open(dot_clangd_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[7] = f'      "-I{Project_src_path}",\n'
lines[8] = f'      "-I{vcpkg_include_path}",\n'
lines[9] = f'      "-I{miniaudio_include_path}",\n'
lines[10] = f'      "-I{whisper_include_path}",\n'
lines[11] = f'      "-I{ggml_include_path}",\n'
with open(dot_clangd_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# project_root/CMakeLists.txt
#
CMakeLists_file = os.path.join(
    Project_root_path, "scripts", "config_files", "CMakeLists.txt"
)
CMakeLists_output_file = os.path.join(Project_root_path, "CMakeLists.txt")
with open(CMakeLists_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
with open(CMakeLists_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# CMakePresets.json
# lines 9, 12, 22, 25
#
CMakePresets_file = os.path.join(
    Project_root_path, "scripts", "config_files", "CMakePresets.json"
)
CMakePresets_file_output_file = os.path.join(Project_root_path, "CMakePresets.json")
with open(CMakePresets_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[8] = f'        "VCPKG_ROOT": "{vcpkg_root_path}"\n'
lines[11] = f'        "CMAKE_TOOLCHAIN_FILE": "{cmake_toolchain_file_path}",\n'
lines[21] = f'        "VCPKG_ROOT": "{vcpkg_root_path}"\n'
lines[24] = f'        "CMAKE_TOOLCHAIN_FILE": "{cmake_toolchain_file_path}",\n'
with open(CMakePresets_file_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)
