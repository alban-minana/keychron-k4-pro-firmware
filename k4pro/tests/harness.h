/* Minimal unit test harness: no dependency beyond the C standard library.
 *
 *   TEST(name) { CHECK(cond); CHECK_EQ(expected, actual); }
 *   int main(void) { RUN(name); return harness_report("suite"); }
 */
#pragma once

#include <stdio.h>

static int harness_failures = 0;
static int harness_checks   = 0;
static int harness_tests    = 0;

#define TEST(name) static void name(void)

#define RUN(name)                \
    do {                         \
        harness_tests++;         \
        test_setup();            \
        name();                  \
    } while (0)

#define CHECK(cond)                                                              \
    do {                                                                         \
        harness_checks++;                                                        \
        if (!(cond)) {                                                           \
            harness_failures++;                                                  \
            printf("  FAIL %s:%d (%s): %s\n", __FILE__, __LINE__, __func__, #cond); \
        }                                                                        \
    } while (0)

#define CHECK_EQ(expected, actual)                                                                  \
    do {                                                                                            \
        long long harness_e_ = (long long)(expected);                                               \
        long long harness_a_ = (long long)(actual);                                                 \
        harness_checks++;                                                                           \
        if (harness_e_ != harness_a_) {                                                             \
            harness_failures++;                                                                     \
            printf("  FAIL %s:%d (%s): %s == %s, expected %lld, got %lld\n", __FILE__, __LINE__,     \
                   __func__, #expected, #actual, harness_e_, harness_a_);                           \
        }                                                                                           \
    } while (0)

static inline int harness_report(const char *suite) {
    printf("%-14s %3d tests, %4d checks, %d failed\n", suite, harness_tests, harness_checks, harness_failures);
    return harness_failures ? 1 : 0;
}
