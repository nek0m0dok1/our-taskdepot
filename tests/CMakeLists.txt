add_executable(foundation_smoke test_foundation.c)
target_include_directories(foundation_smoke PRIVATE "${cjson_SOURCE_DIR}")
target_link_libraries(foundation_smoke PRIVATE cjson unity)
add_custom_command(TARGET foundation_smoke POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SCINTILLA_DLL}" "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll"
  VERBATIM
)
add_test(NAME foundation_smoke COMMAND foundation_smoke "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll")

# Error pattern matching unit tests
add_executable(test_matcher
    test_matcher.c
    ../src/diagnostics/matcher.c
)

target_include_directories(test_matcher PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(test_matcher PRIVATE unity)

add_test(NAME matcher_test COMMAND test_matcher)

# Compile matcher tests using UTF-8
if(MSVC)
    target_compile_options(test_matcher PRIVATE /utf-8)
endif()
