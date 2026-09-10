#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_font.h"
#include "lvgl_theme.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <material_symbols.h>
#include <noto_emoji.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_material_symbols_30_1);
LV_FONT_DECLARE(font_noto_emoji_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_material_symbols_30_1);
    auto emoji_font = std::make_shared<LvglBuiltInFont>(&font_noto_emoji_30_1);

    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);
    dark_theme->set_emoji_font(emoji_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

void OledDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();  // Mark SetupUI as called
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (eye_idle_timer_ != nullptr) {
        esp_timer_stop(eye_idle_timer_);
        esp_timer_delete(eye_idle_timer_);
    }
    if (info_hide_timer_ != nullptr) {
        esp_timer_stop(info_hide_timer_);
        esp_timer_delete(info_hide_timer_);
    }
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }

    bool is_128x64_layout = (top_bar_ != nullptr);
    if (status_bar_ != nullptr && is_128x64_layout) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        if (!is_128x64_layout) {
            status_label_ = nullptr;
            notification_label_ = nullptr;
            network_label_ = nullptr;
            mute_label_ = nullptr;
            battery_label_ = nullptr;
        }
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void OledDisplay::Unlock() { lvgl_port_unlock(); }

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    lv_anim_delete(chat_message_label_, nullptr);
    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::EyeIdleTimerCallback(void* arg) {
    auto* self = static_cast<OledDisplay*>(arg);
    DisplayLockGuard lock(self);
    if (self->eye_left_ == nullptr || self->eye_right_ == nullptr) {
        return;
    }
    // RoboEyes-style idle "saccade": occasionally look off-center, then return.
    int32_t dx = (esp_random() % 13) - 6;   // -6..6
    int32_t dy = (esp_random() % 7) - 3;    // -3..3
    lv_obj_set_pos(self->eye_left_, self->eye_left_base_x_ + dx, self->eye_left_base_y_ + dy);
    lv_obj_set_pos(self->eye_right_, self->eye_right_base_x_ + dx, self->eye_right_base_y_ + dy);
}

void OledDisplay::SetEyesHidden(bool hidden) {
    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(eye_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(eye_right_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(eye_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(eye_right_, LV_OBJ_FLAG_HIDDEN);
    }
}

void OledDisplay::InfoHideTimerCallback(void* arg) {
    auto* self = static_cast<OledDisplay*>(arg);
    DisplayLockGuard lock(self);
    if (self->info_label_ != nullptr) {
        lv_obj_add_flag(self->info_label_, LV_OBJ_FLAG_HIDDEN);
    }
    self->SetEyesHidden(false);
}

void OledDisplay::ShowInfoText(const char* text, int duration_ms) {
    if (info_label_ == nullptr || text == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    lv_label_set_text(info_label_, text);
    lv_obj_align(info_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(info_label_, LV_OBJ_FLAG_HIDDEN);
    SetEyesHidden(true);
    if (info_hide_timer_ != nullptr) {
        esp_timer_stop(info_hide_timer_);
        esp_timer_start_once(info_hide_timer_, (uint64_t)duration_ms * 1000);
    }
}

void OledDisplay::ApplyEyeShape() {
    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }
    int32_t y = eye_center_y_ - eye_open_height_ / 2 + eye_y_offset_;
    lv_obj_set_height(eye_left_, eye_open_height_);
    lv_obj_set_height(eye_right_, eye_open_height_);
    lv_obj_set_y(eye_left_, y);
    lv_obj_set_y(eye_right_, y);
    eye_left_base_y_ = y;
    eye_right_base_y_ = y;
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Fullscreen container, no status bar / clock / wifi / battery */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);

    // RoboEyes-style rounded-rect eyes, absolutely positioned so they can
    // shift around for idle "look around" movement.
    const int32_t eye_w = 34;
    const int32_t eye_h = 40;
    const int32_t gap = 10;
    const int32_t total_w = eye_w * 2 + gap;
    const int32_t start_x = (LV_HOR_RES - total_w) / 2;
    const int32_t start_y = (LV_VER_RES - eye_h) / 2;

    eye_left_base_x_ = start_x;
    eye_left_base_y_ = start_y;
    eye_right_base_x_ = start_x + eye_w + gap;
    eye_right_base_y_ = start_y;
    eye_base_width_ = eye_w;
    eye_base_height_ = eye_h;
    eye_center_y_ = start_y + eye_h / 2;
    eye_open_height_ = eye_h;
    eye_y_offset_ = 0;

    eye_left_ = lv_obj_create(container_);
    lv_obj_set_size(eye_left_, eye_w, eye_h);
    lv_obj_set_pos(eye_left_, eye_left_base_x_, eye_left_base_y_);
    lv_obj_set_style_radius(eye_left_, 12, 0);
    lv_obj_set_style_bg_color(eye_left_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(eye_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    lv_obj_remove_flag(eye_left_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(eye_left_, this);

    eye_right_ = lv_obj_create(container_);
    lv_obj_set_size(eye_right_, eye_w, eye_h);
    lv_obj_set_pos(eye_right_, eye_right_base_x_, eye_right_base_y_);
    lv_obj_set_style_radius(eye_right_, 12, 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(eye_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    lv_obj_remove_flag(eye_right_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(eye_right_, this);

    // Blink animation: shrink height down then back up, pause, repeat forever.
    // Driven as a percentage of the *current* emotion's open height, so
    // SetEmotion() can change eye shape without fighting the blink cycle.
    static lv_anim_t blink_anim;
    lv_anim_init(&blink_anim);
    lv_anim_set_exec_cb(&blink_anim, [](void* var, int32_t value) {
        lv_obj_t* eye = static_cast<lv_obj_t*>(var);
        auto* self = static_cast<OledDisplay*>(lv_obj_get_user_data(eye));
        if (self == nullptr) return;
        int32_t h = self->eye_open_height_ * value / 100;
        if (h < 4) h = 4;
        lv_obj_set_height(eye, h);
        lv_obj_set_y(eye, self->eye_center_y_ - h / 2 + self->eye_y_offset_);
    });
    lv_anim_set_values(&blink_anim, 100, 8);
    lv_anim_set_duration(&blink_anim, 120);
    lv_anim_set_playback_duration(&blink_anim, 120);
    lv_anim_set_repeat_count(&blink_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&blink_anim, 3000);

    lv_anim_set_var(&blink_anim, eye_left_);
    lv_anim_start(&blink_anim);
    lv_anim_set_var(&blink_anim, eye_right_);
    lv_anim_start(&blink_anim);

    // Text overlay used by the info ticker. Hidden until ShowInfoText().
    info_label_ = lv_label_create(container_);
    lv_obj_set_width(info_label_, LV_HOR_RES - 4);
    lv_obj_set_style_pad_all(info_label_, 0, 0);
    lv_label_set_long_mode(info_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(info_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(info_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(info_label_, lv_color_white(), 0);
    lv_label_set_text(info_label_, "");
    lv_obj_align(info_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(info_label_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_create_args_t info_timer_args = {
        .callback = &OledDisplay::InfoHideTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "info_hide",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&info_timer_args, &info_hide_timer_));

    // Idle saccade: periodically glance off-center like RoboEyes' idle mode.
    esp_timer_create_args_t idle_timer_args = {
        .callback = &OledDisplay::EyeIdleTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eye_idle",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&idle_timer_args, &eye_idle_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(eye_idle_timer_, 2500 * 1000));
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, MATERIAL_SYMBOLS_ROBOT_2);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000),
                                   LV_PART_MAIN);
}

void OledDisplay::SetEmotion(const char* emotion) {
    if (eye_left_ != nullptr && eye_right_ != nullptr && emotion != nullptr) {
        DisplayLockGuard lock(this);
        // Map server emotions onto simple RoboEyes-style eye shapes.
        if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "laughing") == 0 ||
            strcmp(emotion, "funny") == 0 || strcmp(emotion, "loving") == 0 ||
            strcmp(emotion, "kissy") == 0 || strcmp(emotion, "relaxed") == 0 ||
            strcmp(emotion, "cool") == 0 || strcmp(emotion, "confident") == 0 ||
            strcmp(emotion, "winking") == 0 || strcmp(emotion, "silly") == 0 ||
            strcmp(emotion, "delicious") == 0) {
            // Happy squint.
            eye_open_height_ = eye_base_height_ * 45 / 100;
            eye_y_offset_ = eye_base_height_ / 6;
        } else if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "crying") == 0 ||
                   strcmp(emotion, "embarrassed") == 0 || strcmp(emotion, "confused") == 0 ||
                   strcmp(emotion, "sleepy") == 0) {
            // Droopy, half-lidded.
            eye_open_height_ = eye_base_height_ * 35 / 100;
            eye_y_offset_ = eye_base_height_ / 3;
        } else if (strcmp(emotion, "angry") == 0 || strcmp(emotion, "shocked") == 0) {
            // Narrowed, slightly raised.
            eye_open_height_ = eye_base_height_ * 55 / 100;
            eye_y_offset_ = -eye_base_height_ / 8;
        } else if (strcmp(emotion, "surprised") == 0) {
            // Wide open.
            eye_open_height_ = eye_base_height_ * 130 / 100;
            eye_y_offset_ = 0;
        } else if (strcmp(emotion, "thinking") == 0) {
            eye_open_height_ = eye_base_height_ * 70 / 100;
            eye_y_offset_ = -eye_base_height_ / 8;
        } else {
            // neutral and anything unrecognized.
            eye_open_height_ = eye_base_height_;
            eye_y_offset_ = 0;
        }
        ApplyEyeShape();
        return;
    }

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const char* utf8 = noto_emoji_get_utf8(emotion);
    const lv_font_t* emotion_font = lvgl_theme->emoji_font()->font();
    if (utf8 == nullptr) {
        utf8 = material_symbols_get_utf8(emotion);
        emotion_font = lvgl_theme->large_icon_font()->font();
    }
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    if (utf8 != nullptr) {
        lv_obj_set_style_text_font(emotion_label_, emotion_font, 0);
        lv_label_set_text(emotion_label_, utf8);
    } else {
        lv_obj_set_style_text_font(emotion_label_, lvgl_theme->emoji_font()->font(), 0);
        lv_label_set_text(emotion_label_, NOTO_EMOJI_NEUTRAL);
    }
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}
