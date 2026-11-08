
#ifndef MATCHER_H
#define MATCHER_H

#include <stddef.h>

/* エラーパターン情報 */
typedef struct {
  const char *id;
  const char *pattern;
  const char *explanation;
  const char *cause;
  int priority;
} ErrorPattern;

/* 照合結果の状態 */
typedef enum { MATCH_FOUND, MATCH_UNSUPPORTED, MATCH_INVALID_INPUT } MatchStatus;

/* 照合結果 */
typedef struct {
  MatchStatus status;
  const ErrorPattern *matched_pattern;
  const char *original_message;
} MatchResult;

/* エラーパターン照合関数 */
MatchResult match_error(const char *message, const ErrorPattern *patterns, size_t pattern_count);

#endif
