add_executable(foundation_smoke test_foundation.c)
target_include_directories(foundation_smoke PRIVATE "${cjson_SOURCE_DIR}")
target_link_libraries(foundation_smoke PRIVATE cjson unity)
add_custom_command(TARGET foundation_smoke POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SCINTILLA_DLL}" "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll"
  VERBATIM
)
add_test(NAME foundation_smoke COMMAND foundation_smoke "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll")

add_executable(compiler_tests test_compiler.c)
target_link_libraries(compiler_tests PRIVATE compiler unity)
target_compile_options(compiler_tests PRIVATE /utf-8)

add_executable(compiler_fixture compiler_fixture.c)
set(_compiler_test_dir "${CMAKE_CURRENT_BINARY_DIR}/compiler-tests/$<CONFIG>")
set_target_properties(compiler_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_compiler_test_dir}")
set_target_properties(compiler_fixture PROPERTIES
  OUTPUT_NAME clang
  RUNTIME_OUTPUT_DIRECTORY "${_compiler_test_dir}/compiler/bin"
)
add_dependencies(compiler_tests compiler_fixture)
add_test(NAME compiler_tests COMMAND compiler_tests "$<TARGET_FILE:compiler_fixture>")
set_tests_properties(compiler_tests PROPERTIES TIMEOUT 30)

set(COMPILER_TEST_CLANG "${PROJECT_SOURCE_DIR}/.tools/llvm-mingw/bin/clang.exe" CACHE FILEPATH
    "LLVM-MinGW clang.exe for compiler integration tests")
if(EXISTS "${COMPILER_TEST_CLANG}")
  add_test(NAME compiler_llvm_mingw COMMAND compiler_tests --llvm-mingw "${COMPILER_TEST_CLANG}")
  set_tests_properties(compiler_llvm_mingw PROPERTIES TIMEOUT 45)
endif()
