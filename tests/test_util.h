/*
 * test_util.h - 최소 테스트 매크로 (외부 라이브러리 없음)
 *
 *   TEST(l2_dark_turns_on) { ... CHECK_EQ(s.sys.out.headlight_on, true); }
 *   int main(void) { RUN(l2_dark_turns_on); return test_summary(); }
 *
 * 실패하면 파일:줄, 실제 값, 기대 값을 출력한다 → PR에 그대로 붙인다.
 */

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int g_checks;
static int g_failures;

#define TEST(name) static void name(void)

#define RUN(name)                      \
    do {                               \
        int before_ = g_failures;      \
        name();                        \
        printf("%s %s\n", g_failures == before_ ? "  PASS" : "  FAIL", #name); \
    } while (0)

#define CHECK(cond)                                                     \
    do {                                                                \
        g_checks++;                                                     \
        if (!(cond)) {                                                  \
            g_failures++;                                               \
            printf("    %s:%d: CHECK(%s) 실패\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        long long a_ = (long long)(actual), e_ = (long long)(expected);     \
        g_checks++;                                                         \
        if (a_ != e_) {                                                     \
            g_failures++;                                                   \
            printf("    %s:%d: %s = %lld, 기대 %lld\n",                     \
                   __FILE__, __LINE__, #actual, a_, e_);                    \
        }                                                                   \
    } while (0)

static inline int test_summary(void)
{
    printf("  %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

#endif /* TEST_UTIL_H */
