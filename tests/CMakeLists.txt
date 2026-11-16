add_executable(foundation_smoke test_foundation.c)
target_include_directories(foundation_smoke PRIVATE "${cjson_SOURCE_DIR}")
target_link_libraries(foundation_smoke PRIVATE cjson unity)
add_custom_command(TARGET foundation_smoke POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${SCINTILLA_DLL}" "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll"
  VERBATIM
)
add_test(NAME foundation_smoke COMMAND foundation_smoke "$<TARGET_FILE_DIR:foundation_smoke>/Scintilla.dll")


# File import unit tests
add_executable(test_file_import
    test_file_import.c
    ../src/file/file_import.c
)

target_include_directories(test_file_import PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(test_file_import PRIVATE unity)

if(MSVC)
    target_compile_options(test_file_import PRIVATE /utf-8)
endif()

add_test(NAME file_import_test COMMAND test_file_import)
