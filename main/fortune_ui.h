// main/fortune_ui.h —— 上上签界面。
//
// 全新设计的四页 LVGL 界面（待机摇签 / 摇签动画 / 卦象结果 / 解签详情），
// 宣纸底 + 墨字 + 朱砂点缀的视觉语言，与基线演示的菜单外壳无任何共享。
// 所有公开函数必须在 LVGL 任务内或持有 bsp_lvgl_lock() 时调用；按键回调
// 路径由 main.c 持锁后进入。
#pragma once

#include <stdbool.h>

#include "bsp_button.h"

// 创建屏幕与四个页面、30 s 周期电量刷新。
// battery_ok=false 或读数为 -1 时隐藏电量标签。
void fortune_ui_init(bool battery_ok);

// 三按键事件分发（已持锁）。语义：
//   待机:   OK 单击 → 开始摇签动画
//   摇签中: OK 单击 → 跳过动画直接出签
//   结果:   OK 单击 → 进入解签
//   解签:   UP/DOWN 滚动翻阅；OK 长按 → 返回待机（可重新摇签）
void fortune_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);
