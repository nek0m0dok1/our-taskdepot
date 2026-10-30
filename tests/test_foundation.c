#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cJSON.h"
#include "unity.h"

static const wchar_t *scintilla_path;

void setUp(void) {
}

void tearDown(void) {
}

static void test_cjson_round_trip(void) {
  const char *input = "{\"ready\":true,\"count\":3}";
  cJSON *root = cJSON_Parse(input);
  TEST_ASSERT_NOT_NULL(root);
  const cJSON *ready = cJSON_GetObjectItemCaseSensitive(root, "ready");
  const cJSON *count = cJSON_GetObjectItemCaseSensitive(root, "count");
  TEST_ASSERT_TRUE(cJSON_IsTrue(ready));
  TEST_ASSERT_NOT_NULL(count);
  TEST_ASSERT_EQUAL_INT(3, count->valueint);

  char *encoded = cJSON_PrintUnformatted(root);
  TEST_ASSERT_NOT_NULL(encoded);
  cJSON *copy = cJSON_Parse(encoded);
  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(copy, "ready")));

  cJSON_Delete(copy);
  cJSON_free(encoded);
  cJSON_Delete(root);
}

static void test_scintilla_dll_loads_and_exports_direct_function(void) {
  HMODULE scintilla = LoadLibraryW(scintilla_path);
  TEST_ASSERT_NOT_NULL(scintilla);
  FARPROC direct_function = GetProcAddress(scintilla, "Scintilla_DirectFunction");
  TEST_ASSERT_NOT_NULL(direct_function);
  FreeLibrary(scintilla);
}

int wmain(int argc, wchar_t **argv) {
  if (argc != 2) {
    return 2;
  }
  scintilla_path = argv[1];
  UNITY_BEGIN();
  RUN_TEST(test_cjson_round_trip);
  RUN_TEST(test_scintilla_dll_loads_and_exports_direct_function);
  return UNITY_END();
}
