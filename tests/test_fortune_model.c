// tests/test_fortune_model.c —— 六十四卦数据表与抽取逻辑宿主测试。
// 编译：cc -std=c11 -Wall -Wextra -Werror -Imain
//       tests/test_fortune_model.c main/fortune_model.c -o /tmp/t
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fortune_model.h"

static int g_failures;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static void test_table_integrity(void) {
    // 文王卦序 1..64 恰好各出现一次；爻位编码六位内且两两不同。
    bool seen_number[FORTUNE_HEXAGRAM_COUNT + 1] = { false };
    bool seen_lines[1 << FORTUNE_LINE_COUNT] = { false };
    for (int i = 0; i < FORTUNE_HEXAGRAM_COUNT; i++) {
        const fortune_hexagram_t *h = fortune_by_number((uint8_t)(i + 1));
        CHECK(h != NULL);
        CHECK(h->number >= 1 && h->number <= FORTUNE_HEXAGRAM_COUNT);
        CHECK(!seen_number[h->number]);
        seen_number[h->number] = true;
        CHECK(h->lines < (1 << FORTUNE_LINE_COUNT));
        CHECK(!seen_lines[h->lines]);
        seen_lines[h->lines] = true;
        CHECK(h->level >= 0 && h->level < FORTUNE_LEVEL_COUNT);
        CHECK(h->name && h->name[0] != '\0');
        CHECK(h->full_name && h->full_name[0] != '\0');
        CHECK(h->guoci && h->guoci[0] != '\0');
        CHECK(h->baihua && h->baihua[0] != '\0');
        // 卦名至多两个汉字（UTF-8 每字 3 字节）。
        CHECK(strlen(h->name) <= 6);
    }
    for (int n = 1; n <= FORTUNE_HEXAGRAM_COUNT; n++) {
        CHECK(seen_number[n]);
    }
}

static void test_line_encoding_anchors(void) {
    // 爻序方向锚点：bit0 = 初爻（最下）。乾坤两极。
    const fortune_hexagram_t *qian = fortune_by_number(1);
    CHECK(strcmp(qian->name, "乾") == 0);
    CHECK(qian->lines == 0x3F); // 六爻皆阳
    const fortune_hexagram_t *kun = fortune_by_number(2);
    CHECK(strcmp(kun->name, "坤") == 0);
    CHECK(kun->lines == 0x00); // 六爻皆阴
    // 剥：仅上爻为阳（bit5）——验证最上爻方向。
    const fortune_hexagram_t *bo = fortune_by_number(23);
    CHECK(strcmp(bo->name, "剥") == 0);
    CHECK(bo->lines == 0x20);
    CHECK(fortune_line_yang(bo, 5));
    CHECK(!fortune_line_yang(bo, 0));
    // 复：仅初爻为阳（bit0）——验证最下爻方向。
    const fortune_hexagram_t *fu = fortune_by_number(24);
    CHECK(strcmp(fu->name, "复") == 0);
    CHECK(fu->lines == 0x01);
    CHECK(fortune_line_yang(fu, 0));
    CHECK(!fortune_line_yang(fu, 5));
    // 既济：初、三、五爻为阳，阴阳间隔自下而起。
    const fortune_hexagram_t *jiji = fortune_by_number(63);
    CHECK(strcmp(jiji->name, "既济") == 0);
    CHECK(jiji->lines == 0x15);
    // 未济：初、三、五爻为阴。
    const fortune_hexagram_t *weiji = fortune_by_number(64);
    CHECK(strcmp(weiji->name, "未济") == 0);
    CHECK(weiji->lines == 0x2A);
    // 越界爻位安全返回 false；越界卦号安全返回 NULL。
    CHECK(!fortune_line_yang(bo, 6));
    CHECK(!fortune_line_yang(NULL, 0));
    CHECK(fortune_by_number(0) == NULL);
    CHECK(fortune_by_number(FORTUNE_HEXAGRAM_COUNT + 1) == NULL);
}

static void test_draw_logic(void) {
    // 抽签确定性：同 seed 同卦；大范围扫描覆盖全部 64 卦且四级皆现。
    const fortune_hexagram_t *a = fortune_draw(12345U);
    const fortune_hexagram_t *b = fortune_draw(12345U);
    CHECK(a == b);
    bool covered[FORTUNE_HEXAGRAM_COUNT + 1] = { false };
    size_t level_hits[FORTUNE_LEVEL_COUNT] = { 0 };
    for (uint32_t seed = 0; seed < FORTUNE_HEXAGRAM_COUNT * 100U; seed++) {
        const fortune_hexagram_t *h = fortune_draw(seed);
        CHECK(h != NULL);
        CHECK(h->number >= 1 && h->number <= FORTUNE_HEXAGRAM_COUNT);
        covered[h->number] = true;
        level_hits[h->level]++;
    }
    for (int n = 1; n <= FORTUNE_HEXAGRAM_COUNT; n++) {
        CHECK(covered[n]);
    }
    for (int lv = 0; lv < FORTUNE_LEVEL_COUNT; lv++) {
        CHECK(level_hits[lv] > 0);
    }
}

static void test_level_labels(void) {
    for (int lv = 0; lv < FORTUNE_LEVEL_COUNT; lv++) {
        const char *label = fortune_level_label((fortune_level_t)lv);
        const char *hint = fortune_level_hint((fortune_level_t)lv);
        CHECK(label && label[0] != '\0');
        CHECK(hint && hint[0] != '\0');
    }
    CHECK(fortune_level_label(FORTUNE_LEVEL_COUNT) == NULL);
    CHECK(fortune_level_hint(FORTUNE_LEVEL_COUNT) == NULL);
}

int main(void) {
    test_table_integrity();
    test_line_encoding_anchors();
    test_draw_logic();
    test_level_labels();
    if (g_failures != 0) {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("fortune model: all checks passed (%d hexagrams)\n",
           FORTUNE_HEXAGRAM_COUNT);
    return 0;
}
