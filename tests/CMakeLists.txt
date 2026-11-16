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


# Windows file selection dialog
add_library(file_dialog STATIC
    ../src/file/file_dialog.c
)

target_include_directories(file_dialog PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(file_dialog PUBLIC comdlg32)

if(MSVC)
    target_compile_options(file_dialog PRIVATE /utf-8)
endif()

# Manual test for Windows file dialog and file import
add_executable(manual_file_dialog
    manual_file_dialog.c
    ../src/file/file_import.c
)

target_include_directories(manual_file_dialog PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(manual_file_dialog PRIVATE file_dialog)

if(MSVC)
    target_compile_options(manual_file_dialog PRIVATE /utf-8)
endif()

# Editor state unit tests
add_executable(test_editor_state
    test_editor_state.c
    ../src/file/editor_state.c
)

target_include_directories(test_editor_state PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(test_editor_state PRIVATE unity)

if(MSVC)
    target_compile_options(test_editor_state PRIVATE /utf-8)
endif()

add_test(NAME editor_state_test COMMAND test_editor_state)
