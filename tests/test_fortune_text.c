// tests/test_fortune_text.c —— 上上签 UI 字符串与字体码点清单一致性（宿主可跑，
// 不依赖 LVGL）。防止清单漂移：main/fortune_text.h 与 main/fortune_model.c 中
// 每个字符串的每个码点都必须出现在 assets/fonts/fortune_chars.txt（或 ASCII
// 0x20-0x7E 区段）中；同时强制 main/fortune_ui.c 与 main/main.c 不出现任何
// 非字符串外的裸中文（界面中文只允许来自 text.h 与卦签数据）。
// 编译：cc -std=c11 -Wall -Wextra -Werror
//       tests/test_fortune_text.c -o /tmp/t 然后在仓库根目录运行 /tmp/t。
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CODEPOINTS 4096
#define KNOWN_MISSING 0x9F98U // 龍：繁体，故意不在清单中的负例
#define KNOWN_PRESENT 0x4E0AU // 上：标题锚点，防止空清单漏检

static int g_failures;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("FAIL: cannot open %s (run from repository root)\n", path);
        g_failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        printf("FAIL: cannot read %s\n", path);
        g_failures++;
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

// 去除 // 与注释，保留字符串字面量（含引号）。返回新缓冲（malloc）。
static char *strip_comments(const char *text) {
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    if (!out) {
        printf("FAIL: out of memory\n");
        g_failures++;
        return NULL;
    }
    size_t o = 0;
    bool in_string = false;
    bool in_line = false;
    bool in_block = false;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        char next = i + 1 < n ? text[i + 1] : '\0';
        if (in_line) {
            if (c == '\n') {
                in_line = false;
                out[o++] = c;
            }
            continue;
        }
        if (in_block) {
            if (c == '*' && next == '/') {
                in_block = false;
                i++;
            }
            continue;
        }
        if (in_string) {
            out[o++] = c;
            if (c == '\\' && next != '\0') {
                out[o++] = next;
                i++;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
            out[o++] = c;
            continue;
        }
        if (c == '/' && next == '/') {
            in_line = true;
            i++;
            continue;
        }
        if (c == '/' && next == '*') {
            in_block = true;
            i++;
            continue;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

// 严格 UTF-8 解码；非法序列返回 -1。
static int decode_utf8(const unsigned char *s, uint32_t *cp) {
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    int len = s[0] < 0xE0 ? 2 : (s[0] < 0xF0 ? 3 : 4);
    if (len == 2 && s[0] < 0xC2) return -1;
    uint32_t value = len == 2 ? (uint32_t)(s[0] & 0x1F)
                             : len == 3 ? (uint32_t)(s[0] & 0x0F)
                                        : (uint32_t)(s[0] & 0x07);
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) return -1;
        value = (value << 6) | (uint32_t)(s[i] & 0x3F);
    }
    if (len == 3 && value < 0x800) return -1;
    if (len == 4 && value < 0x10000) return -1;
    *cp = value;
    return len;
}

static uint32_t s_allowed[MAX_CODEPOINTS];
static size_t s_allowed_count;

static int allowed_contains(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return 1; // ASCII 由 --range 隐式覆盖
    for (size_t i = 0; i < s_allowed_count; i++) {
        if (s_allowed[i] == cp) return 1;
    }
    return 0;
}

static void load_inventory(void) {
    char *inventory = read_file("assets/fonts/fortune_chars.txt");
    if (!inventory) return;
    for (char *line = strtok(inventory, "\n"); line;
         line = strtok(NULL, "\n")) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        uint32_t cp;
        if (sscanf(line, " 0x%8x", &cp) == 1 || sscanf(line, "0x%8x", &cp) == 1) {
            if (s_allowed_count < MAX_CODEPOINTS) {
                s_allowed[s_allowed_count++] = cp;
            }
        }
    }
    free(inventory);
}

// 逐字符串扫描：must_be_ascii=true 时任何高位码点都算失败（红线检查）。
static void scan_source(const char *path, size_t *strings, size_t *codepoints,
                        bool must_be_ascii) {
    char *raw = read_file(path);
    if (!raw) return;
    char *text = strip_comments(raw);
    free(raw);
    if (!text) return;
    for (char *p = text; *p;) {
        if (*p != '"') {
            p++;
            continue;
        }
        char *quote = p + 1;
        (*strings)++;
        while (*quote && *quote != '"') {
            uint32_t cp;
            int len = decode_utf8((const unsigned char *)quote, &cp);
            if (len < 0) {
                printf("FAIL: invalid UTF-8 in %s near \"%.8s\"\n", path,
                       quote);
                g_failures++;
                break;
            }
            if (must_be_ascii) {
                if (cp > 0x7E) {
                    printf("FAIL: non-ASCII U+%04X in %s (UI code must take "
                           "text from fortune_text.h / fortune_model.c)\n",
                           cp, path);
                    g_failures++;
                }
            } else if (!allowed_contains(cp)) {
                printf("FAIL: U+%04X not in font inventory "
                       "(fortune_chars.txt)\n",
                       cp);
                g_failures++;
            }
            (*codepoints)++;
            quote += len;
        }
        p = *quote ? quote + 1 : quote;
    }
    free(text);
}

int main(void) {
    // 1) 读入清单（# 注释 + 每行一个 0xXXXX）。
    load_inventory();
    CHECK(s_allowed_count >= 500); // 空清单或截断都应失败

    // 锚点：已知存在与已知缺失。
    CHECK(allowed_contains(KNOWN_PRESENT));
    CHECK(!allowed_contains(KNOWN_MISSING));

    // 2) 展示文本来源：UI 字符串 + 六十四卦数据，必须全覆盖。
    size_t strings = 0;
    size_t codepoints = 0;
    scan_source("main/fortune_text.h", &strings, &codepoints, false);
    size_t data_strings = 0;
    size_t data_codepoints = 0;
    scan_source("main/fortune_model.c", &data_strings, &data_codepoints,
                false);
    strings += data_strings;
    codepoints += data_codepoints;

    // 3) 红线：界面代码与入口不得出现裸中文字符串。
    size_t unused = 0;
    scan_source("main/fortune_ui.c", &unused, &unused, true);
    scan_source("main/main.c", &unused, &unused, true);

    CHECK(strings >= 130); // 16 条 UI 文案 + 64 卦 x 4 字段
    CHECK(codepoints >= 1000);

    if (g_failures != 0) {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("fortune text coverage: %zu string(s), %zu codepoint(s), all "
           "covered\n",
           strings, codepoints);
    return 0;
}
