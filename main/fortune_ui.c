// main/fortune_ui.c —— 上上签界面：待机 / 摇签动画 / 卦象 / 解签四页。
//
// 视觉：宣纸米色底 + 墨色文字 + 朱砂/鎏金点缀。签筒为抗锯齿位图素材
//（assets/images/generate_images.py 生成，RGB565A8），摇签动画用 pivot
// 旋转摆动；卦画保持绘制回调像素画（圆角爻 + 鎏金边框）。
#include "fortune_ui.h"

#include <stdbool.h>

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
#define COLOR_TEXT_ON_BADGE 0xF7F1E3
#define COLOR_FRAME 0xC9A85C     // 卦画鎏金边框（浅一档）
#define COLOR_SHADOW 0x5C4A3A    // 徽章投影

#define BATTERY_PERIOD_MS 30000
#define SHAKE_DURATION_MS 2600
#define SWAY_PERIOD_MS 280
#define SWAY_ANGLE 150           // 0.1 度单位，±1.5 度
#define DETAIL_PAGE_STEP 276     // 滚动容器可视高度，一按翻一页

typedef enum {
    UI_IDLE = 0,  // 待机：按 OK 摇签
    UI_DRAWING,   // 摇签动画中
    UI_RESULT,    // 卦象结果
    UI_DETAIL,    // 解签详情（可滚动）
} ui_page_t;

static const char *TAG = "fortune_ui";

extern const lv_image_dsc_t fortune_img_tube;

static ui_page_t s_page;
static const fortune_hexagram_t *s_hex;
static bool s_battery_ok;

static lv_obj_t *s_scr;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_idle_page;
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
static lv_timer_t *s_finish_timer;
static bool s_sway_running;

// ---------------------------------------------------------------------------
// 卦画绘制（圆角爻 + 鎏金边框）
// ---------------------------------------------------------------------------

static void draw_pixel_rect(lv_layer_t *layer, const lv_area_t *base, int x,
                            int y, int w, int h, uint32_t color, int radius) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.base.layer = layer;
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = (int16_t)radius;
    lv_area_t area = {
        .x1 = base->x1 + x,
        .y1 = base->y1 + y,
        .x2 = base->x1 + x + w - 1,
        .y2 = base->y1 + y + h - 1,
    };
    lv_draw_rect(layer, &dsc, &area);
}

// 单个爻段：阳爻整条圆角，阴爻两段圆角（中空 8px）。
static void draw_bar(lv_layer_t *layer, const lv_area_t *base, int x, int y,
                     int w, bool yang) {
    if (yang) {
        draw_pixel_rect(layer, base, x, y, w, 12, COLOR_INK, 6);
    } else {
        int seg = (w - 8) / 2;
        draw_pixel_rect(layer, base, x, y, seg, 12, COLOR_INK, 6);
        draw_pixel_rect(layer, base, x + w - seg, y, seg, 12, COLOR_INK, 6);
    }
}

// 卦画像素画（层 112x72）：上卦、下卦两柱左右并排（上卦在左，与卦名
// 读序一致），柱内自上而下为上爻在顶；外围一圈鎏金细框。
static void bars_draw_cb(lv_event_t *event) {
    if (!s_hex) return;
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);

    const int ROW_H = 12;
    const int ROW_GAP = 10;
    const int COL_W = 44;
    const int COL_GAP = 8;
    const int PAD = 8;
    // 柱内行序：上爻在顶。上卦 = 四、五、上爻；下卦 = 初、二、三爻。
    static const uint8_t UPPER_LINES[3] = { 5, 4, 3 };
    static const uint8_t LOWER_LINES[3] = { 2, 1, 0 };
    for (int row = 0; row < 3; row++) {
        int y = PAD + row * (ROW_H + ROW_GAP);
        draw_bar(layer, &base, PAD, y, COL_W,
                 fortune_line_yang(s_hex, UPPER_LINES[row]));
        draw_bar(layer, &base, PAD + COL_W + COL_GAP, y, COL_W,
                 fortune_line_yang(s_hex, LOWER_LINES[row]));
    }

    // 鎏金纸窗式边框。
    lv_draw_rect_dsc_t frame;
    lv_draw_rect_dsc_init(&frame);
    frame.base.layer = layer;
    frame.bg_opa = LV_OPA_TRANSP;
    frame.border_color = lv_color_hex(COLOR_FRAME);
    frame.border_width = 2;
    frame.border_opa = LV_OPA_COVER;
    frame.radius = 10;
    lv_draw_rect(layer, &frame, &base);
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
    lv_obj_set_style_text_line_space(obj, 6, 0);
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

static void set_tube_image(lv_obj_t *parent, int x, int y) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &fortune_img_tube);
    lv_obj_set_pos(img, x, y);
}

// 详情页小节标题：朱砂竖条 + 鎏金文字。
static lv_obj_t *make_section_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *row = make_blank_obj(parent, 0, 0, 208, 26);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    lv_obj_t *bar = lv_obj_create(row);
    lv_obj_set_size(bar, 4, 20);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_CINNABAR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &app_font_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_GOLD), 0);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_label_set_text(label, text);
    return row;
}

// ---------------------------------------------------------------------------
// 页面构建
// ---------------------------------------------------------------------------

static void build_idle_page(void) {
    s_idle_page = make_page();

    lv_obj_t *title = make_label(s_idle_page, &app_font_48, COLOR_INK);
    lv_label_set_text(title, FORTUNE_TXT_TITLE);
    lv_obj_set_pos(title, 0, 42);
    lv_obj_set_style_text_letter_space(title, 6, 0);

    lv_obj_t *rule = make_blank_obj(s_idle_page, 84, 104, 72, 4);
    lv_obj_set_style_bg_color(rule, lv_color_hex(COLOR_CINNABAR), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rule, 2, 0);

    lv_obj_t *subtitle = make_label(s_idle_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(subtitle, FORTUNE_TXT_SUBTITLE);
    lv_obj_set_pos(subtitle, 0, 118);
    lv_obj_set_style_text_letter_space(subtitle, 3, 0);

    set_tube_image(s_idle_page, 60, 152);

    lv_obj_t *hint = make_label(s_idle_page, &app_font_24, COLOR_CINNABAR);
    lv_label_set_text(hint, FORTUNE_TXT_PRESS_OK);
    lv_obj_set_pos(hint, 0, 292);
    lv_obj_set_style_text_letter_space(hint, 2, 0);
}

static void build_draw_page(void) {
    s_draw_page = make_page();

    // 摇签位图签筒：pivot 设在底部中心，摆动时像手持晃动。
    s_draw_tube = lv_image_create(s_draw_page);
    lv_image_set_src(s_draw_tube, &fortune_img_tube);
    lv_obj_set_pos(s_draw_tube, 60, 116);
    lv_image_set_pivot(s_draw_tube, 60, 124);

    lv_obj_t *status = make_label(s_draw_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(status, FORTUNE_TXT_SHAKING);
    lv_obj_set_pos(status, 0, 288);
    lv_obj_set_style_text_letter_space(status, 3, 0);
}

static void build_result_page(void) {
    s_result_page = make_page();

    s_result_num = make_label(s_result_page, &app_font_24, COLOR_DIM);
    lv_obj_set_pos(s_result_num, 0, 16);

    s_result_bars = make_blank_obj(s_result_page, 64, 52, 112, 72);
    lv_obj_add_event_cb(s_result_bars, bars_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    s_result_name = make_label(s_result_page, &app_font_48, COLOR_INK);
    lv_obj_set_pos(s_result_name, 0, 134);
    lv_obj_set_style_text_letter_space(s_result_name, 4, 0);

    // 吉凶徽章：垂直渐变 + 轻阴影，等级色在刷新时应用。
    s_result_badge = make_blank_obj(s_result_page, 60, 200, 120, 36);
    lv_obj_set_style_radius(s_result_badge, 18, 0);
    lv_obj_set_style_bg_opa(s_result_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s_result_badge, 8, 0);
    lv_obj_set_style_shadow_opa(s_result_badge, LV_OPA_40, 0);
    lv_obj_set_style_shadow_offset_y(s_result_badge, 2, 0);
    lv_obj_set_style_shadow_color(s_result_badge, lv_color_hex(COLOR_SHADOW),
                                  0);
    s_result_badge_label =
        make_label(s_result_badge, &app_font_24, COLOR_TEXT_ON_BADGE);
    lv_obj_set_width(s_result_badge_label, 120);
    lv_obj_set_style_text_letter_space(s_result_badge_label, 3, 0);
    lv_obj_center(s_result_badge_label);

    lv_obj_t *hint = make_label(s_result_page, &app_font_24, COLOR_CINNABAR);
    lv_label_set_text(hint, FORTUNE_TXT_SEE_DETAIL);
    lv_obj_set_pos(hint, 0, 284);
    lv_obj_set_style_text_letter_space(hint, 2, 0);
}

static void build_detail_page(void) {
    s_detail_page = make_page();

    s_detail_scroll = lv_obj_create(s_detail_page);
    lv_obj_set_pos(s_detail_scroll, 0, 0);
    lv_obj_set_size(s_detail_scroll, 240, 276);
    lv_obj_set_style_bg_opa(s_detail_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_detail_scroll, 0, 0);
    lv_obj_set_style_pad_top(s_detail_scroll, 4, 0);
    lv_obj_set_style_pad_bottom(s_detail_scroll, 12, 0);
    lv_obj_set_style_pad_left(s_detail_scroll, 0, 0);
    lv_obj_set_style_pad_right(s_detail_scroll, 0, 0);
    lv_obj_set_scrollbar_mode(s_detail_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_detail_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_detail_scroll, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_detail_scroll, 12, 0);

    s_detail_name = make_label(s_detail_scroll, &app_font_48, COLOR_INK);
    lv_obj_set_style_text_letter_space(s_detail_name, 4, 0);
    s_detail_num = make_label(s_detail_scroll, &app_font_24, COLOR_DIM);
    s_detail_level = make_label(s_detail_scroll, &app_font_24, COLOR_CINNABAR);
    lv_obj_set_style_text_letter_space(s_detail_level, 2, 0);
    s_detail_hint_text = make_label(s_detail_scroll, &app_font_24, COLOR_GOLD);

    make_section_label(s_detail_scroll, FORTUNE_TXT_GUOCI_LABEL);
    s_detail_guoci_text = make_wrap_label(s_detail_scroll, COLOR_INK);

    make_section_label(s_detail_scroll, FORTUNE_TXT_BAIHUA_LABEL);
    s_detail_baihua_text = make_wrap_label(s_detail_scroll, COLOR_INK);

    lv_obj_t *back = make_label(s_detail_page, &app_font_24, COLOR_DIM);
    lv_label_set_text(back, FORTUNE_TXT_BACK_HINT);
    lv_obj_set_pos(back, 0, 288);
}

// ---------------------------------------------------------------------------
// 刷新与切换
// ---------------------------------------------------------------------------

static void apply_badge_colors(fortune_level_t level) {
    // 每级一对（亮、暗）渐变色，鎏金/朱砂/棕/灰。
    uint32_t hi = COLOR_GOLD;
    uint32_t lo = 0x8A651A;
    switch (level) {
        case FORTUNE_LEVEL_SHANG_SHANG:
            hi = 0xC08A28;
            lo = 0x8A651A;
            break;
        case FORTUNE_LEVEL_SHANG:
            hi = 0xC2493C;
            lo = 0x8F2C22;
            break;
        case FORTUNE_LEVEL_ZHONG:
            hi = 0x9A8A6A;
            lo = 0x6F6350;
            break;
        case FORTUNE_LEVEL_XIA:
            hi = 0x5A544E;
            lo = 0x383430;
            break;
        default:
            break;
    }
    lv_obj_set_style_bg_color(s_result_badge, lv_color_hex(hi), 0);
    lv_obj_set_style_bg_grad_color(s_result_badge, lv_color_hex(lo), 0);
    lv_obj_set_style_bg_grad_dir(s_result_badge, LV_GRAD_DIR_VER, 0);
}

static void refresh_result(void) {
    if (!s_hex) return;
    lv_label_set_text_fmt(s_result_num, FORTUNE_TXT_NO_FMT,
                          (int)s_hex->number);
    lv_label_set_text(s_result_name, s_hex->name);
    apply_badge_colors(s_hex->level);
    lv_label_set_text(s_result_badge_label,
                      fortune_level_label(s_hex->level));
    lv_obj_invalidate(s_result_bars);
}

static void refresh_detail(void) {
    if (!s_hex) return;
    lv_label_set_text(s_detail_name, s_hex->name);
    lv_label_set_text_fmt(s_detail_num, FORTUNE_TXT_FULL_NO_FMT,
                          s_hex->full_name, (int)s_hex->number);
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

// 摆动动画：以签筒底部为轴做 ±1.5 度快速小幅摆动。
static void sway_anim_cb(void *var, int32_t angle) {
    lv_image_set_rotation((lv_obj_t *)var, angle);
}

static void sway_stop(void) {
    if (s_sway_running) {
        lv_anim_del(s_draw_tube, sway_anim_cb);
        lv_image_set_rotation(s_draw_tube, 0);
        s_sway_running = false;
    }
    if (s_finish_timer) {
        lv_timer_del(s_finish_timer);
        s_finish_timer = NULL;
    }
}

static void reveal_result(void) {
    sway_stop();
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
    set_page(UI_DRAWING);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_draw_tube);
    lv_anim_set_exec_cb(&a, sway_anim_cb);
    lv_anim_set_values(&a, -SWAY_ANGLE, SWAY_ANGLE);
    lv_anim_set_duration(&a, SWAY_PERIOD_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_reverse_duration(&a, SWAY_PERIOD_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    s_sway_running = true;

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
                                 ? cur + DETAIL_PAGE_STEP
                                 : cur - DETAIL_PAGE_STEP;
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
    s_sway_running = false;
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
