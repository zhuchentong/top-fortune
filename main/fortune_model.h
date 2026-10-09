// main/fortune_model.h —— 上上签：六十四卦灵签数据与抽取纯逻辑。
//
// 本模块不依赖 ESP-IDF/LVGL，宿主可直接编译测试：
//   cc -std=c11 -Wall -Wextra -Werror -Imain
//     tests/test_fortune_model.c main/fortune_model.c -o /tmp/t 然后 /tmp/t。
// 随机源由调用方注入（设备上用 esp_random() 混合时间戳），保证可测性。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FORTUNE_MODEL_VERSION 1

// 六十四卦全量数据（文王卦序），任一 seed 都抽自同一张表。
#define FORTUNE_HEXAGRAM_COUNT 64

// 每卦六爻；bit0 = 初爻（最下），bit5 = 上爻（最上），1 = 阳爻。
#define FORTUNE_LINE_COUNT 6

typedef enum {
    FORTUNE_LEVEL_SHANG_SHANG = 0, // 上上签 · 大吉
    FORTUNE_LEVEL_SHANG,           // 上签 · 吉
    FORTUNE_LEVEL_ZHONG,           // 中签 · 平
    FORTUNE_LEVEL_XIA,             // 下签 · 慎
    FORTUNE_LEVEL_COUNT
} fortune_level_t;

typedef struct {
    uint8_t number;        // 文王卦序 1..64
    uint8_t lines;         // 六爻位编码，见 FORTUNE_LINE_COUNT 注释
    fortune_level_t level; // 灵签吉凶等级
    const char *name;      // 卦名，如 "乾"、"中孚"
    const char *full_name; // 卦象名，如 "乾为天"、"风泽中孚"
    const char *guoci;     // 卦辞原文（节引《周易》）
    const char *baihua;    // 白话解签
} fortune_hexagram_t;

// 抽签：seed → 卦（确定性纯函数，均匀覆盖全表）。
const fortune_hexagram_t *fortune_draw(uint32_t seed);

// 按文王卦序号查卦；越界（0 或 >64）返回 NULL。
const fortune_hexagram_t *fortune_by_number(uint8_t number);

// 查询第 line 爻（0 = 初爻 … 5 = 上爻）是否为阳爻；line >= 6 返回 false。
bool fortune_line_yang(const fortune_hexagram_t *hexagram, uint8_t line);

// 吉凶等级的签名与短提示（返回值来自 fortune_text.h，静态存储期）。
const char *fortune_level_label(fortune_level_t level);
const char *fortune_level_hint(fortune_level_t level);
