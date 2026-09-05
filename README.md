# Metasequoia IME Engine(水杉输入法引擎)

General IME engine shared by the Metasequoia IME frontends: [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows) (through [MSIME-Server](https://github.com/metasequoiaime/MSIME-Server)), [MSIME-Apple](https://github.com/metasequoiaime/MSIME-Apple) and [MSIME-Linux](https://github.com/metasequoiaime/MSIME-Linux).

## How to build

Prerequisites:

- Visual Studio 2026
- CMake 3.21+
- Python 3.10+
- vcpkg
- Git

Build steps:

```powershell
git clone --recursive https://github.com/metasequoiaime/MSIME-Engine.git
cd .\MSIME-Engine\
python .\tests\scripts\prepare_env.py
cd .\tests\
.\scripts\llaunch.ps1
```

Then, you could check the outputs in the terminal.
