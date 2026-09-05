if(NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR NOT MSIME_VOICE_CAPTURE)
    message(FATAL_ERROR "This host example requires macOS and MSIME_VOICE_CAPTURE")
endif()
add_executable(MSIMEVoiceExample MACOSX_BUNDLE main.mm)
set_target_properties(MSIMEVoiceExample PROPERTIES
    OBJCXX_STANDARD 17
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
target_compile_options(MSIMEVoiceExample PRIVATE -fobjc-arc -Wall -Wextra -Werror)
target_link_libraries(MSIMEVoiceExample PRIVATE MetasequoiaIme::Voice MetasequoiaIme::VoiceCapture "-framework AppKit" "-framework AVFoundation")
