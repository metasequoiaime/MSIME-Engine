cmake_minimum_required(VERSION 3.25.0)

cmake_policy(SET CMP0167 OLD)

project(MetasequoiaImeEngineTest VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# For utf8cpp, make it use C++11 and later
add_compile_options(/Zc:__cplusplus)

# Boost
set(Boost_ROOT "C:/Users/SonnyCalcr/scoop/apps/boost/current")
set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS locale)
include_directories(${Boost_INCLUDE_DIRS})

# fmt, spdlog and sqlite3
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

include_directories("./includes/")
include_directories("../")
include_directories("../utfcpp/source/")

set(MY_EXECUTABLE_NAME "imetest")
set(
    SOURCE_FILES
    "./src/test_pinyin.cpp"
    "../core/ime_session.cpp"
    "../providers/pinyin_candidate_provider.cpp"
    "../providers/wubi_candidate_provider.cpp"
    "../providers/provider_registry.cpp"
    "../schemes/shuangpin_scheme.cpp"
    "../schemes/quanpin_scheme.cpp"
    "../schemes/wubi_scheme.cpp"
    "../quanpin/engine.cpp"
    "../quanpin/quanpin_dictionary.cpp"
    "../quanpin/quanpin_query.cpp"
    "../quanpin/quanpin_utils.cpp"
    "../quanpin/word_lattice.cpp"
    "../shuangpin/engine.cpp"
    "../common/helpcode_utils.cpp"
    "../common/string_utils.cpp"
    "../shuangpin/shuangpin_dictionary.cpp"
    "../shuangpin/shuangpin_profile.cpp"
    "../shuangpin/shuangpin_query.cpp"
    "../shuangpin/shuangpin_utils.cpp"
    # Google IME
    "../googlepinyinime-rev/src/share/dictbuilder.cpp"
    "../googlepinyinime-rev/src/share/dictlist.cpp"
    "../googlepinyinime-rev/src/share/dicttrie.cpp"
    "../googlepinyinime-rev/src/share/lpicache.cpp"
    "../googlepinyinime-rev/src/share/matrixsearch.cpp"
    "../googlepinyinime-rev/src/share/mystdlib.cpp"
    "../googlepinyinime-rev/src/share/ngram.cpp"
    "../googlepinyinime-rev/src/share/pinyinime.cpp"
    "../googlepinyinime-rev/src/share/searchutility.cpp"
    "../googlepinyinime-rev/src/share/spellingtable.cpp"
    "../googlepinyinime-rev/src/share/spellingtrie.cpp"
    "../googlepinyinime-rev/src/share/splparser.cpp"
    "../googlepinyinime-rev/src/share/sync.cpp"
    "../googlepinyinime-rev/src/share/userdict.cpp"
    "../googlepinyinime-rev/src/share/utf16char.cpp"
    "../googlepinyinime-rev/src/share/utf16reader.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES} )

target_link_libraries(${MY_EXECUTABLE_NAME} fmt::fmt spdlog::spdlog unofficial::sqlite3::sqlite3 Boost::locale)
