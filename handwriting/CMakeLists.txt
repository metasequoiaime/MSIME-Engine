add_library(MetasequoiaHandwriting STATIC
  handwriting.cpp
  third_party/zinnia/character.cpp
  third_party/zinnia/feature.cpp
  third_party/zinnia/recognizer.cpp
  third_party/zinnia/sexp.cpp)
add_library(MetasequoiaIme::Handwriting ALIAS MetasequoiaHandwriting)
target_include_directories(MetasequoiaHandwriting PUBLIC ../include)
target_compile_definitions(MetasequoiaHandwriting PRIVATE HAVE_SYS_TYPES_H HAVE_SYS_STAT_H HAVE_FCNTL_H HAVE_STRING_H)
if(WIN32)
  # zinnia.h decorates Character::create/Recognizer::create with __declspec(dllimport) unless told the sources are built into a static library, which makes call sites look for __imp_ symbols that this archive does not export.
  target_compile_definitions(MetasequoiaHandwriting PRIVATE HAVE_WINDOWS_H NOMINMAX ZINNIA_STATIC_LIBRARY)
else()
  target_compile_definitions(MetasequoiaHandwriting PRIVATE HAVE_UNISTD_H HAVE_SYS_MMAN_H HAVE_MMAP)
endif()
if(BUILD_TESTING)
  add_executable(MetasequoiaHandwritingTests test_handwriting.cpp)
  target_link_libraries(MetasequoiaHandwritingTests PRIVATE MetasequoiaHandwriting)
  add_test(NAME MetasequoiaHandwritingTests COMMAND MetasequoiaHandwritingTests "${CMAKE_CURRENT_SOURCE_DIR}/models/handwriting-zh_CN.model")
endif()
