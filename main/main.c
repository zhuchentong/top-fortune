// main/main.c —— 上上签：初始化 + 直进应用 + 按键分发。
//
// 启动顺序：显示/LVGL（失败即退出）→ 背光 100% → 按键 → 电量（软失败
// 不阻塞）→ 创建界面。本应用无音频、无持久化，开机即待机摇签页。
// 按键语义见 fortune_ui.h。
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "fortune_ui.h"

static const char *TAG = "top-fortune";

// 按键回调运行在 button 组件的共享 esp_timer 任务里：只做有界分发，
// LVGL 访问全部在持锁之后（回调内绝不做慢操作——非阻塞红线）。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    fortune_ui_on_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "boot");

    // 屏幕是本应用的 UI 载体，失败没有界面可言——打清日志后退出。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed, cannot continue");
        return;
    }
    bsp_display_backlight(100);

    // 按键：无按键则应用不可操作，但保留界面显示以暴露状态。
    esp_err_t button_err = bsp_button_init(on_key, NULL);
    if (button_err != ESP_OK) {
        ESP_LOGE(TAG, "button init failed: %s", esp_err_to_name(button_err));
    }

    // 电量软失败：仅电量角标降级，摇签功能不受影响。
    bool battery_ok = bsp_battery_init() == ESP_OK;
    if (!battery_ok) {
        ESP_LOGW(TAG, "battery init failed, gauge hidden");
    }

    if (bsp_lvgl_lock(1000)) {
        fortune_ui_init(battery_ok);
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "ready");
}
