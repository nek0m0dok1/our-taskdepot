
#include "matcher.h"

#include <string.h>

/* 診断メッセージと登録済みパターンを照合する */
MatchResult match_error(const char *message, const ErrorPattern *patterns, size_t pattern_count) {
  MatchResult result = {MATCH_INVALID_INPUT, NULL, message};

  /* 診断メッセージがNULLまたは空文字列の場合 */
  if (message == NULL || message[0] == '\0') {
    return result;
  }

  /* パターン件数が1件以上なのに配列がNULLの場合 */
  if (patterns == NULL && pattern_count > 0) {
    return result;
  }

  /* 初期状態では未対応エラーとして扱う */
  result.status = MATCH_UNSUPPORTED;

  /* 登録済みパターンを順番に確認する */
  for (size_t i = 0; i < pattern_count; i++) {
    const ErrorPattern *current = &patterns[i];

    /* 使用できないパターンを除外する */
    if (current->id == NULL || current->pattern == NULL || current->pattern[0] == '\0') {
      continue;
    }

    /* 診断メッセージにパターンが含まれているか確認 */
    if (strstr(message, current->pattern) == NULL) {
      continue;
    }

    /* 最初に一致したパターンを記録する */
    if (result.matched_pattern == NULL) {
      result.matched_pattern = current;
      result.status = MATCH_FOUND;
      continue;
    }

    /* 現在選択中のパターンと比較する */
    const ErrorPattern *best = result.matched_pattern;

    /* 優先順位が高いパターンを採用する */
    if (current->priority > best->priority ||
        (current->priority == best->priority && strlen(current->pattern) > strlen(best->pattern))) {
      result.matched_pattern = current;
    }
  }

  return result;
}
