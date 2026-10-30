add_executable(foundation_smoke test_foundation.c)
target_include_directories(foundation_smoke PRIVATE "${cjson_SOURCE_DIR}")
target_link_libraries(foundation_smoke PRIVATE cjson unity)
add_custom_command(TARGET foundation_smoke POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SCINTILLA_DLL}" "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll"
  VERBATIM
)
add_test(NAME foundation_smoke COMMAND foundation_smoke "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll")
