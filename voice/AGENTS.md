# AGENTS.md - MetasequoiaVoiceInput Development Guide

## Project Overview

Voice input module for MetasequoiaIME. Uses Whisper for local STT or SiliconFlow cloud API, with Silero VAD for voice activity detection.

## Build Commands

### Prerequisites

- Visual Studio 2022 or later
- vcpkg (configured in `C:/Users/SonnyCalcr/scoop/apps/vcpkg/current`)
- CMake 3.25+

### Build Configuration

```powershell
# Debug build (preset: vcpkg)
cmake --preset vcmpkg
cmake --build build --config Debug

# Release build (preset: vcmpkg-release)
cmake --preset vcmpkg-release
cmake --build build --config Release

# Or use Visual Studio
cmake --preset vcmpkg
cmake --build build --config Debug
```

### Running the Application

```powershell
# Debug
.\build\bin\Debug\MetasequoiaVoiceInput.exe

# Release
.\build\bin\Release\MetasequoiaVoiceInput.exe
```

### Testing

This project does not have a unit test suite. Manual testing is done by running the executable and speaking.

## Code Style Guidelines

### General

- **C++ Standard**: C++17
- **Platform**: Windows-only (uses Windows API)
- **Encoding**: UTF-8 (console set with `SetConsoleOutputCP(CP_UTF8)`)

### Formatting

- **Style**: Based on Microsoft with modifications
- **Indent Width**: 4 spaces
- **Column Limit**: 300 characters
- **Include Sorting**: Disabled (keep original order)

### Naming Conventions

| Element          | Convention                 | Example                         |
| ---------------- | -------------------------- | ------------------------------- |
| Classes          | PascalCase                 | `WhisperWorker`, `AudioCapture` |
| Functions        | PascalCase                 | `start()`, `recognize()`        |
| Member Variables | trailing underscore        | `ctx_`, `callback_`             |
| Namespaces       | lowercase with underscores | `mvi_utils`                     |
| Enums            | PascalCase with enum class | `SttProvider::LocalWhisper`     |
| Constants        | PascalCase                 | `g_stt_provider`                |

### File Organization

- Header files: `.h` extension
- Implementation files: `.cpp` extension
- Source directory: `src/`
- One class per file (or logical grouping)
- Order: headers, then implementation

### Include Order (recommended)

1. Project headers (local quotes)
2. Standard library headers (`<>`)
3. Third-party headers (`<>`)

```cpp
#include "whisper_worker.h"
#include "stt_service.h"
#include <string>
#include <vector>
#include <fmt/format.h>
```

### Header Guards

- Use `#pragma once` (not `#ifndef` guards)

### Class Layout

```cpp
class ClassName : public BaseClass
{
  public:
    // Constructor/destructor
    explicit ClassName(Type param);
    ~ClassName();

    // Public methods
    ReturnType method() override;

  private:
    // Member variables
    Type member_;
};
```

### Error Handling

- Use `std::exception` and derived types
- Always catch exceptions in callback/thread contexts
- Use try-catch in main entry point

```cpp
try
{
    // operation
}
catch (const std::exception& e)
{
    printf("[ERROR] %s\n", e.what());
}
catch (...)
{
    printf("[ERROR] Unknown exception\n");
}
```

### Dependencies (vcpkg)

- `fmt` - formatting library
- `onnxruntime` - ONNX runtime for VAD
- `curl` - HTTP client for cloud STT
- `nlohmann-json` - JSON parsing
- `whisper.cpp` - local speech recognition (third_party)
- `miniaudio` - audio capture (third_party)

### clangd Configuration

The `.clangd` file provides language server configuration. Key flags:

- `-std=c++17`
- Unicode defines: `-DUNICODE`, `-D_UNICODE`
- Include paths for vcpkg and third_party

### Important Notes

- Member variables use trailing underscore (`_`)
- Use `explicit` for single-argument constructors
- Prefer `std::unique_ptr` over raw pointers for ownership
- Use `override` keyword when overriding virtual methods
- Use `enum class` instead of plain enums
- For Windows API, use wide strings (`std::wstring`) where required

### Logging

- Use `printf()` with `fflush(stdout)` for console output
- Format: `[CATEGORY] message`
- Examples: `[INIT]`, `[STT]`, `[CALLBACK ERROR]`

### CMake Conventions

- Source files explicitly listed (not GLOB)
- Use `PRIVATE` for target_link_libraries
- Set `CMAKE_RUNTIME_OUTPUT_DIRECTORY` for executable location
- Use `find_package` with `CONFIG REQUIRED` for vcpkg packages
