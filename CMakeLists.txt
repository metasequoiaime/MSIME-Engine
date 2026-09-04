cmake_minimum_required(VERSION 3.25)

project(MetasequoiaImeEngine VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

if(TARGET SQLite3::SQLite3)
    set(METASEQUOIA_IME_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(METASEQUOIA_IME_SQLITE_TARGET SQLite::SQLite3)
endif()

get_target_property(METASEQUOIA_IME_SPDLOG_INCLUDE_DIRS spdlog::spdlog_header_only INTERFACE_INCLUDE_DIRECTORIES)

set(METASEQUOIA_IME_ENGINE_SOURCES
    common/helpcode_utils.cpp
    common/string_utils.cpp
    core/ime_session.cpp
    core/input_session.cpp
    english/english_dictionary.cpp
    japanese/japanese_matrix_search.cpp
    japanese/japanese_sentence_decoder.cpp
    japanese/romaji_converter.cpp
    providers/japanese_candidate_provider.cpp
    providers/pinyin_candidate_provider.cpp
    providers/provider_registry.cpp
    providers/wubi_candidate_provider.cpp
    quanpin/engine.cpp
    quanpin/quanpin_dictionary.cpp
    quanpin/quanpin_query.cpp
    quanpin/quanpin_utils.cpp
    schemes/japanese_romaji_scheme.cpp
    schemes/quanpin_scheme.cpp
    schemes/shuangpin_scheme.cpp
    schemes/wubi_scheme.cpp
    shuangpin/engine.cpp
    shuangpin/shuangpin_dictionary.cpp
    shuangpin/shuangpin_profile.cpp
    shuangpin/shuangpin_query.cpp
    shuangpin/shuangpin_utils.cpp
    user_dictionary/user_dictionary_journal.cpp
    googlepinyinime-rev/src/share/dictbuilder.cpp
    googlepinyinime-rev/src/share/dictlist.cpp
    googlepinyinime-rev/src/share/dicttrie.cpp
    googlepinyinime-rev/src/share/lpicache.cpp
    googlepinyinime-rev/src/share/matrixsearch.cpp
    googlepinyinime-rev/src/share/mystdlib.cpp
    googlepinyinime-rev/src/share/ngram.cpp
    googlepinyinime-rev/src/share/pinyinime.cpp
    googlepinyinime-rev/src/share/searchutility.cpp
    googlepinyinime-rev/src/share/spellingtable.cpp
    googlepinyinime-rev/src/share/spellingtrie.cpp
    googlepinyinime-rev/src/share/splparser.cpp
    googlepinyinime-rev/src/share/sync.cpp
    googlepinyinime-rev/src/share/userdict.cpp
    googlepinyinime-rev/src/share/utf16char.cpp
    googlepinyinime-rev/src/share/utf16reader.cpp
)

add_library(MetasequoiaImeEngine STATIC ${METASEQUOIA_IME_ENGINE_SOURCES})
add_library(MetasequoiaIme::Engine ALIAS MetasequoiaImeEngine)

target_include_directories(MetasequoiaImeEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/utfcpp/source
    SYSTEM PRIVATE
        ${METASEQUOIA_IME_SPDLOG_INCLUDE_DIRS}
)

target_compile_definitions(MetasequoiaImeEngine PRIVATE FMT_HEADER_ONLY=1 SPDLOG_FMT_EXTERNAL)

target_link_libraries(MetasequoiaImeEngine
    PUBLIC
        Boost::headers
        fmt::fmt-header-only
        ${METASEQUOIA_IME_SQLITE_TARGET}
        Threads::Threads
)

if(WIN32)
    target_link_libraries(MetasequoiaImeEngine PUBLIC ole32 shell32)
endif()

if(MSVC)
    target_compile_definitions(MetasequoiaImeEngine PUBLIC NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_compile_options(MetasequoiaImeEngine PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(MetasequoiaImeEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(test_platform_api tests/src/test_platform_api.cpp)
    target_link_libraries(test_platform_api PRIVATE MetasequoiaIme::Engine)
    add_test(NAME platform_api COMMAND test_platform_api)

    add_executable(test_data_path tests/src/test_data_path.cpp)
    target_link_libraries(test_data_path PRIVATE MetasequoiaIme::Engine)
    add_test(NAME data_path COMMAND test_data_path)

    add_executable(test_engine_smoke tests/src/test_engine_smoke.cpp)
    target_link_libraries(test_engine_smoke PRIVATE MetasequoiaIme::Engine)
    add_test(NAME engine_smoke COMMAND test_engine_smoke)

    add_executable(test_input_session tests/src/test_input_session.cpp)
    target_link_libraries(test_input_session PRIVATE MetasequoiaIme::Engine)
    add_test(NAME input_session COMMAND test_input_session)
endif()
