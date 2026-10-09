// main/fortune_text.h —— 上上签全部界面字符串的唯一来源。
//
// 红线：界面不允许出现本文件（及 fortune_model.c 卦签数据）之外的中文。
// 新增字符串时必须：
//   1. 运行 assets/fonts/collect_chars.py 重新生成 fortune_chars.txt；
//   2. 用 assets/fonts/generate_fonts.sh 重新生成两个字库；
//   3. tests/test_fortune_text.c 会强制校验覆盖，缺失即宿主测试失败。
#pragma once

#define FORTUNE_TXT_TITLE            "上上签"
#define FORTUNE_TXT_SUBTITLE         "易经六十四卦灵签"
#define FORTUNE_TXT_PRESS_OK         "按 OK 摇签"
#define FORTUNE_TXT_SHAKING          "诚心摇签…"
#define FORTUNE_TXT_NO_FMT           "第 %d 卦"
#define FORTUNE_TXT_SEE_DETAIL       "按 OK 解签"
#define FORTUNE_TXT_GUOCI_LABEL      "卦辞"
#define FORTUNE_TXT_BAIHUA_LABEL     "白话解签"
#define FORTUNE_TXT_BACK_HINT        "长按 OK 重摇"
#define FORTUNE_TXT_LEVEL_SHANG_SHANG "上上签"
#define FORTUNE_TXT_LEVEL_SHANG       "上签"
#define FORTUNE_TXT_LEVEL_ZHONG       "中签"
#define FORTUNE_TXT_LEVEL_XIA         "下签"
#define FORTUNE_TXT_HINT_SHANG_SHANG  "大吉 · 诸事亨通"
#define FORTUNE_TXT_HINT_SHANG        "吉 · 诸事可为"
#define FORTUNE_TXT_HINT_ZHONG        "平 · 谨慎稳行"
#define FORTUNE_TXT_HINT_XIA          "慎 · 守正待时"
