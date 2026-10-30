include(FetchContent)

set(_versions_file "${CMAKE_SOURCE_DIR}/tools/versions.json")
if(NOT EXISTS "${_versions_file}")
  message(FATAL_ERROR "Missing dependency manifest: ${_versions_file}")
endif()
file(READ "${_versions_file}" _versions_json)

function(_manifest_library_field library field output)
  string(JSON _value ERROR_VARIABLE _error GET "${_versions_json}" libraries "${library}" "${field}")
  if(NOT _error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "tools/versions.json: libraries.${library}.${field}: ${_error}")
  endif()
  set(${output} "${_value}" PARENT_SCOPE)
endfunction()

foreach(_library IN ITEMS cjson unity)
  foreach(_field IN ITEMS version url sha256 provenance)
    _manifest_library_field("${_library}" "${_field}" _${_library}_${_field})
    if(_${_library}_${_field} STREQUAL "")
      message(FATAL_ERROR "tools/versions.json: libraries.${_library}.${_field} must not be empty")
    endif()
  endforeach()
  if(NOT _${_library}_url MATCHES "^https://")
    message(FATAL_ERROR "libraries.${_library}.url must use HTTPS")
  endif()
  if(NOT _${_library}_provenance MATCHES "^https://")
    message(FATAL_ERROR "libraries.${_library}.provenance must be an HTTPS source")
  endif()
  string(LENGTH "${_${_library}_sha256}" _sha256_length)
  if(NOT _sha256_length EQUAL 64 OR NOT _${_library}_sha256 MATCHES "^[0-9A-Fa-f]+$")
    message(FATAL_ERROR "libraries.${_library}.sha256 must be a 64-character SHA-256")
  endif()
endforeach()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "Build cJSON tests" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "Build cJSON utilities" FORCE)
set(ENABLE_CJSON_VERSION_SO OFF CACHE BOOL "Build cJSON version shared object" FORCE)
set(ENABLE_TARGET_EXPORT OFF CACHE BOOL "Export cJSON targets" FORCE)
set(ENABLE_LOCALES OFF CACHE BOOL "Build cJSON locale tests" FORCE)
FetchContent_Declare(cjson
  URL "${_cjson_url}"
  URL_HASH "SHA256=${_cjson_sha256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(unity
  URL "${_unity_url}"
  URL_HASH "SHA256=${_unity_sha256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cjson unity)

set(_scintilla_version "")
string(JSON _scintilla_version ERROR_VARIABLE _scintilla_error GET "${_versions_json}" tools scintilla version)
if(NOT _scintilla_error STREQUAL "NOTFOUND")
  message(FATAL_ERROR "tools/versions.json: tools.scintilla.version: ${_scintilla_error}")
endif()
if(NOT _scintilla_version STREQUAL "5.6.7")
  message(FATAL_ERROR "ADR-0001 pins Scintilla 5.6.7; manifest specifies '${_scintilla_version}'")
endif()
set(SCINTILLA_DLL "${CMAKE_SOURCE_DIR}/.tools/scintilla/wscite/Scintilla.dll" CACHE FILEPATH
    "Verified x64 Scintilla.dll from the pinned official SciTE archive")
set(SCINTILLA_LICENSE_FILE "${CMAKE_SOURCE_DIR}/.tools/scintilla/wscite/License.txt")
include("${CMAKE_SOURCE_DIR}/cmake/ValidateScintilla.cmake")
