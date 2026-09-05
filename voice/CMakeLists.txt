cmake_minimum_required(VERSION 3.25.0)
project(MetasequoiaVoiceInput VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Windows-specific settings
if(MSVC)
    add_definitions(
        /D_UNICODE=1
        /DUNICODE=1
        /DNOMINMAX
    )
endif()

# Dependencies
find_package(fmt CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

# miniaudio
include_directories(third_party/miniaudio)

# Source files (Explicit list to avoid GLOB issues)
set(SOURCE_FILES
    "src/audio_capture.cpp"
    "src/audio_capture.h"
    "src/cloud_stt_worker.cpp"
    "src/cloud_stt_worker.h"
    "src/cue_player.cpp"
    "src/cue_player.h"
    "src/main.cpp"
    "src/resource/resource.h"
    "src/resource/app.rc"
    "src/mvi_config.cpp"
    "src/mvi_config.h"
    "src/mvi_utils.cpp"
    "src/mvi_utils.h"
    "src/send_input.cpp"
    "src/send_input.h"
    "src/stt_service.h"
    "src/text_polisher.cpp"
    "src/text_polisher.h"
    "src/vad.cpp"
    "src/vad.h"
    "src/wave_overlay.cpp"
    "src/wave_overlay.h"
    "src/wav_writer.h"
    "src/window_webview2.cpp"
    "src/window_webview2.h"
)

set(MY_EXECUTABLE_NAME "MetasequoiaVoiceInput")
add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES})

# Build as a GUI app so no console window is shown at runtime.
if(MSVC)
    target_link_options(${MY_EXECUTABLE_NAME}
        PRIVATE
        /SUBSYSTEM:WINDOWS
        /ENTRY:mainCRTStartup
    )
endif()

# Link libraries
target_link_libraries(${MY_EXECUTABLE_NAME}
    PRIVATE
    fmt::fmt
    CURL::libcurl
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    d2d1
    User32
    dwmapi
)

# Target include directories
target_include_directories(${MY_EXECUTABLE_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include
)
