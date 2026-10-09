// main/fortune_ui.c —— 上上签界面：待机 / 摇签动画 / 卦象 / 解签四页。
//
// 视觉：宣纸米色底 + 墨色文字 + 朱砂/鎏金点缀，签筒与卦画均为绘制回调
// 像素画（该屏无 PSRAM，避免大块离屏缓冲）。摇签动画只用单层局部重绘。
#include "fortune_ui.h"

#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "fortune_model.h"
#include "fortune_text.h"
#include "lvgl.h"

LV_FONT_DECLARE(app_font_48);
LV_FONT_DECLARE(app_font_24);

// 全新视觉：宣纸底 + 墨字 + 朱砂（与基线演示和深色演示均无关）。
#define COLOR_BG 0xF3ECD9        // 宣纸
#define COLOR_INK 0x3B2F2F       // 墨
#define COLOR_DIM 0x8C7B6A       // 淡墨
#define COLOR_CINNABAR 0xB03A2E  // 朱砂
#define COLOR_GOLD 0xA67B1E      // 鎏金
#define COLOR_TAN 0x8A7A5A       // 中签徽底
#define COLOR_SLATE 0x4A4440     // 下签徽底
#define COLOR_TEXT_ON_BADGE 0xF7F1E3
#define COLOR_TUBE 0x7A4E23      // 竹筒
#define COLOR_TUBE_LIGHT 0x9A6A38
#define COLOR_TUBE_DARK 0x5C3A1A
#define COLOR_STICK 0xD8C08A     // 竹签

#define BATTERY_PERIOD_MS 30000
#define SHAKE_DURATION_MS 2600
#define SHAKE_FRAME_MS 80
#define DETAIL_SCROLL_STEP 40

typedef enum {
    UI_IDLE = 0,  // 待机：按 OK 摇签
    UI_DRAWING,   // 摇签动画中
    UI_RESULT,    // 卦象结果
    UI_DETAIL,    // 解签详情（可滚动）
} ui_page_t;

static const char *TAG = "fortune_ui";

static ui_page_t s_page;
static const fortune_hexagram_t *s_hex;
static bool s_battery_ok;

static lv_obj_t *s_scr;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_idle_page;
static lv_obj_t *s_idle_tube;
static lv_obj_t *s_draw_page;
static lv_obj_t *s_draw_tube;
static lv_obj_t *s_result_page;
static lv_obj_t *s_result_num;
static lv_obj_t *s_result_bars;
static lv_obj_t *s_result_name;
static lv_obj_t *s_result_badge;
static lv_obj_t *s_result_badge_label;
static lv_obj_t *s_detail_page;
static lv_obj_t *s_detail_scroll;
static lv_obj_t *s_detail_name;
static lv_obj_t *s_detail_num;
static lv_obj_t *s_detail_level;
static lv_obj_t *s_detail_hint_text;
static lv_obj_t *s_detail_guoci_text;
static lv_obj_t *s_detail_baihua_text;

static lv_timer_t *s_battery_timer;
static lv_timer_t *s_shake_timer;
static lv_timer_t *s_finish_timer;
static uint32_t s_frame;

// ---------------------------------------------------------------------------
// 绘制基元
// ---------------------------------------------------------------------------

static void draw_pixel_rect(lv_layer_t *layer, const lv_area_t *base, int x,
                            int y, int w, int h, uint32_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.base.layer = layer;
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_area_t area = {
        .x1 = base->x1 + x,
        .y1 = base->y1 + y,
        .x2 = base->x1 + x + w - 1,
        .y2 = base->y1 + y + h - 1,
    };
    lv_draw_rect(layer, &dsc, &area);
}

// 摇签晃动：整体左右摆 + 签杆上下抖（确定性表驱动，帧进自增）。
static const int8_t WOBBLE[8] = { 0, 3, 5, 3, 0, -3, -5, -3 };
static const int8_t JITTER[8] = { 0, -2, 1, -1, 2, 0, -2, 1 };

// 签筒像素画（层 120x132）。state==UI_DRAWING 时叠加晃动与签杆抖动。
static void draw_tube(lv_layer_t *layer, const lv_area_t *base) {
    bool shaking = s_page == UI_DRAWING;
    int dx = shaking ? WOBBLE[(s_frame / 2) & 7] : 0;

    // 签杆：5 支，顶端露在筒口上方；第 3 支朱砂签头。
    static const int8_t STICK_X[5] = { 28, 44, 60, 76, 88 };
    static const int8_t STICK_TOP[5] = { 12, 5, 15, 7, 18 };
    for (int k = 0; k < 5; k++) {
        int jitter = shaking ? JITTER[(s_frame + k * 3) & 7] : 0;
        int top = STICK_TOP[k] + jitter;
        if (top < 0) top = 0;
        draw_pixel_rect(layer, base, STICK_X[k] + dx / 2, top, 6, 48 - top,
                        COLOR_STICK);
        if (k == 2) {
            draw_pixel_rect(layer, base, STICK_X[k] + dx / 2, top, 6, 5,
                            COLOR_CINNABAR);
        }
    }

    // 筒口沿 → 筒身 → 明暗 → 束带 → 圈足。
    draw_pixel_rect(layer, base, 16 + dx, 42, 88, 12, COLOR_TUBE_DARK);
    draw_pixel_rect(layer, base, 24 + dx, 52, 72, 66, COLOR_TUBE);
    draw_pixel_rect(layer, base, 28 + dx, 52, 4, 66, COLOR_TUBE_LIGHT);
    draw_pixel_rect(layer, base, 88 + dx, 52, 4, 66, COLOR_TUBE_DARK);
    draw_pixel_rect(layer, base, 24 + dx, 62, 72, 8, COLOR_TUBE_DARK);
    draw_pixel_rect(layer, base, 24 + dx, 70, 72, 2, COLOR_GOLD);
    draw_pixel_rect(layer, base, 34 + dx, 118, 52, 8, COLOR_TUBE_DARK);
}

static void tube_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    draw_tube(layer, &base);
}

// 卦画像素画（层 104x90）：上爻在上、初爻在下；阳爻整条，阴爻两段。
static void bars_draw_cb(lv_event_t *event) {
    if (!s_hex) return;
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);

    const int ROW_H = 10;
    const int ROW_GAP = 8;
    for (uint8_t row = 0; row < FORTUNE_LINE_COUNT; row++) {
        uint8_t line = FORTUNE_LINE_COUNT - 1U - row; // 上爻画在最上
        int y = (int)row * (ROW_H + ROW_GAP);
        if (fortune_line_yang(s_hex, line)) {
            draw_pixel_rect(layer, &base, 0, y, 104, ROW_H, COLOR_INK);
        } else {
            draw_pixel_rect(layer, &base, 0, y, 44, ROW_H, COLOR_INK);
            draw_pixel_rect(layer, &base, 60, y, 44, ROW_H, COLOR_INK);
        }
    }
}

// ---------------------------------------------------------------------------
// 通用构件
// ---------------------------------------------------------------------------

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_width(obj, 240);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static lv_obj_t *make_wrap_label(lv_obj_t *parent, uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, &app_font_24, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_width(obj, 208);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    return obj;
}

static lv_obj_t *make_blank_obj(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *make_page(void) {
    return make_blank_obj(s_scr, 0, 0, 240, 320);
}

static void set_hidden(lv_obj_t *obj, bool hidden) {
    if (!obj) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// 页面构建
// ---------------------------------------------------------------------------

static void build_idle_page(void) {
    s_idle_page = make_page();

    lv_obj_t *title = make_label(s_idle_page, &app_font_48, COLOR_INK);
    lv_label_set_text(title, FORTUNE_TXT_TITLE);
    lv_obj_set_pos(title, 0, 44);

    lv_obj_t *rule = make_blank_obj(s_idle_page, 84, 106, 72, 4);
    lv_obj_set_style_bg_color(rule, lv_color_hex(COLOR_CINNABAR), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

    lv_obj_t *subtitle = make_label(s_idle_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(subtitle, FORTUNE_TXT_SUBTITLE);
    lv_obj_set_pos(subtitle, 0, 120);

    s_idle_tube = make_blank_obj(s_idle_page, 60, 156, 120, 132);
    lv_obj_add_event_cb(s_idle_tube, tube_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *hint = make_label(s_idle_page, &app_font_24, COLOR_CINNABAR);
    lv_label_set_text(hint, FORTUNE_TXT_PRESS_OK);
    lv_obj_set_pos(hint, 0, 292);
}

static void build_draw_page(void) {
    s_draw_page = make_page();

    s_draw_tube = make_blank_obj(s_draw_page, 60, 120, 120, 132);
    lv_obj_add_event_cb(s_draw_tube, tube_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *status = make_label(s_draw_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(status, FORTUNE_TXT_SHAKING);
    lv_obj_set_pos(status, 0, 288);
}

static void build_result_page(void) {
    s_result_page = make_page();

    s_result_num = make_label(s_result_page, &app_font_24, COLOR_DIM);
    lv_obj_set_pos(s_result_num, 0, 18);

    s_result_bars = make_blank_obj(s_result_page, 68, 52, 104, 90);
    lv_obj_add_event_cb(s_result_bars, bars_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    s_result_name = make_label(s_result_page, &app_font_48, COLOR_INK);
    lv_obj_set_pos(s_result_name, 0, 152);

    s_result_badge = make_blank_obj(s_result_page, 60, 216, 120, 36);
    lv_obj_set_style_radius(s_result_badge, 18, 0);
    lv_obj_set_style_bg_opa(s_result_badge, LV_OPA_COVER, 0);
    s_result_badge_label =
        make_label(s_result_badge, &app_font_24, COLOR_TEXT_ON_BADGE);
    lv_obj_set_width(s_result_badge_label, 120);
    lv_obj_center(s_result_badge_label);

    lv_obj_t *hint = make_label(s_result_page, &app_font_24, COLOR_CINNABAR);
    lv_label_set_text(hint, FORTUNE_TXT_SEE_DETAIL);
    lv_obj_set_pos(hint, 0, 284);
}

static void build_detail_page(void) {
    s_detail_page = make_page();

    s_detail_scroll = lv_obj_create(s_detail_page);
    lv_obj_set_pos(s_detail_scroll, 0, 0);
    lv_obj_set_size(s_detail_scroll, 240, 276);
    lv_obj_set_style_bg_opa(s_detail_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_detail_scroll, 0, 0);
    lv_obj_set_style_pad_all(s_detail_scroll, 0, 0);
    lv_obj_set_scrollbar_mode(s_detail_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_detail_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_detail_scroll, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_detail_scroll, 8, 0);

    s_detail_name = make_label(s_detail_scroll, &app_font_48, COLOR_INK);
    s_detail_num = make_label(s_detail_scroll, &app_font_24, COLOR_DIM);
    s_detail_level = make_label(s_detail_scroll, &app_font_24, COLOR_CINNABAR);
    s_detail_hint_text = make_label(s_detail_scroll, &app_font_24, COLOR_GOLD);

    lv_obj_t *guoci_label =
        make_label(s_detail_scroll, &app_font_24, COLOR_GOLD);
    lv_label_set_text(guoci_label, FORTUNE_TXT_GUOCI_LABEL);
    s_detail_guoci_text = make_wrap_label(s_detail_scroll, COLOR_INK);

    lv_obj_t *baihua_label =
        make_label(s_detail_scroll, &app_font_24, COLOR_GOLD);
    lv_label_set_text(baihua_label, FORTUNE_TXT_BAIHUA_LABEL);
    s_detail_baihua_text = make_wrap_label(s_detail_scroll, COLOR_INK);

    lv_obj_t *back = make_label(s_detail_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(back, FORTUNE_TXT_BACK_HINT);
    lv_obj_set_pos(back, 0, 288);
}

// ---------------------------------------------------------------------------
// 刷新与切换
// ---------------------------------------------------------------------------

static uint32_t badge_color(fortune_level_t level) {
    switch (level) {
        case FORTUNE_LEVEL_SHANG_SHANG: return COLOR_GOLD;
        case FORTUNE_LEVEL_SHANG: return COLOR_CINNABAR;
        case FORTUNE_LEVEL_ZHONG: return COLOR_TAN;
        default: return COLOR_SLATE;
    }
}

static void refresh_result(void) {
    if (!s_hex) return;
    lv_label_set_text_fmt(s_result_num, FORTUNE_TXT_NO_FMT,
                          (int)s_hex->number);
    lv_label_set_text(s_result_name, s_hex->name);
    lv_obj_set_style_bg_color(s_result_badge,
                              lv_color_hex(badge_color(s_hex->level)), 0);
    lv_label_set_text(s_result_badge_label,
                      fortune_level_label(s_hex->level));
    lv_obj_invalidate(s_result_bars);
}

static void refresh_detail(void) {
    if (!s_hex) return;
    lv_label_set_text(s_detail_name, s_hex->name);
    lv_label_set_text_fmt(s_detail_num, FORTUNE_TXT_NO_FMT,
                          (int)s_hex->number);
    lv_label_set_text(s_detail_level, fortune_level_label(s_hex->level));
    lv_label_set_text(s_detail_hint_text, fortune_level_hint(s_hex->level));
    lv_label_set_text(s_detail_guoci_text, s_hex->guoci);
    lv_label_set_text(s_detail_baihua_text, s_hex->baihua);
    lv_obj_scroll_to_y(s_detail_scroll, 0, LV_ANIM_OFF);
}

static void set_page(ui_page_t page) {
    // 无条件应用可见性，避免开机或快速切换时页面叠印。
    s_page = page;
    set_hidden(s_idle_page, page != UI_IDLE);
    set_hidden(s_draw_page, page != UI_DRAWING);
    set_hidden(s_result_page, page != UI_RESULT);
    set_hidden(s_detail_page, page != UI_DETAIL);
}

// ---------------------------------------------------------------------------
// 摇签流程
// ---------------------------------------------------------------------------

// 帧驱动：12.5 fps 推进晃动相位并只重绘签筒小块。
static void shake_timer_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_draw_tube) return;
    s_frame++;
    lv_obj_invalidate(s_draw_tube);
}

static void shake_stop(void) {
    if (s_shake_timer) {
        lv_timer_del(s_shake_timer);
        s_shake_timer = NULL;
    }
    if (s_finish_timer) {
        lv_timer_del(s_finish_timer);
        s_finish_timer = NULL;
    }
}

static void reveal_result(void) {
    shake_stop();
    refresh_result();
    set_page(UI_RESULT);
}

static void finish_timer_cb(lv_timer_t *timer) {
    (void)timer;
    s_finish_timer = NULL;
    if (s_page == UI_DRAWING) reveal_result();
}

static void draw_start(void) {
    // 硬件随机数混合上电以来的时间戳，避开可预测的固定序列。
    uint32_t seed = esp_random() ^ (uint32_t)esp_timer_get_time();
    s_hex = fortune_draw(seed);
    s_frame = 0;
    set_page(UI_DRAWING);

    s_shake_timer = lv_timer_create(shake_timer_cb, SHAKE_FRAME_MS, NULL);
    s_finish_timer = lv_timer_create(finish_timer_cb, SHAKE_DURATION_MS, NULL);
    lv_timer_set_repeat_count(s_finish_timer, 1);
}

// ---------------------------------------------------------------------------
// 按键与定时
// ---------------------------------------------------------------------------

void fortune_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (!s_scr) return;

    switch (s_page) {
        case UI_IDLE:
            if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) draw_start();
            break;
        case UI_DRAWING:
            if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) reveal_result();
            break;
        case UI_RESULT:
            if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
                refresh_detail();
                set_page(UI_DETAIL);
            }
            break;
        case UI_DETAIL: {
            if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
                set_page(UI_IDLE);
                break;
            }
            if (ev != BSP_BTN_CLICK ||
                (btn != BSP_BTN_UP && btn != BSP_BTN_DOWN)) {
                break;
            }
            int32_t cur = lv_obj_get_scroll_y(s_detail_scroll);
            int32_t bottom = lv_obj_get_scroll_bottom(s_detail_scroll);
            int32_t target = btn == BSP_BTN_DOWN
                                 ? cur + DETAIL_SCROLL_STEP
                                 : cur - DETAIL_SCROLL_STEP;
            if (target > bottom) target = bottom;
            if (target < 0) target = 0;
            if (target != cur) {
                lv_obj_scroll_to_y(s_detail_scroll, target, LV_ANIM_ON);
            }
            break;
        }
    }
}

static void refresh_battery(void) {
    if (!s_battery_ok) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        // 项目规则：读不到就隐藏，绝不显示 -1。
        set_hidden(s_battery_label, true);
        return;
    }
    lv_label_set_text_fmt(s_battery_label, "%d%%", soc);
    set_hidden(s_battery_label, false);
}

static void battery_timer_cb(lv_timer_t *timer) {
    (void)timer;
    refresh_battery();
}

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------

void fortune_ui_init(bool battery_ok) {
    s_battery_ok = battery_ok;
    s_page = UI_IDLE;
    s_hex = NULL;
    s_frame = 0;
    s_shake_timer = NULL;
    s_finish_timer = NULL;

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_battery_label = make_label(s_scr, &app_font_24, COLOR_DIM);
    lv_obj_set_width(s_battery_label, LV_SIZE_CONTENT);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -8, 12);
    set_hidden(s_battery_label, true);

    build_idle_page();
    build_draw_page();
    build_result_page();
    build_detail_page();

    set_page(UI_IDLE);
    s_battery_timer = lv_timer_create(battery_timer_cb, BATTERY_PERIOD_MS,
                                      NULL);
    lv_screen_load(s_scr);
    ESP_LOGI(TAG, "top-fortune UI ready");
}
