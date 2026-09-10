#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>


class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* info_label_ = nullptr;
    esp_timer_handle_t eye_idle_timer_ = nullptr;
    esp_timer_handle_t info_hide_timer_ = nullptr;
    int32_t eye_left_base_x_ = 0;
    int32_t eye_left_base_y_ = 0;
    int32_t eye_right_base_x_ = 0;
    int32_t eye_right_base_y_ = 0;
    int32_t eye_base_width_ = 34;
    int32_t eye_base_height_ = 40;
    int32_t eye_center_y_ = 0;
    int32_t eye_open_height_ = 40;
    int32_t eye_y_offset_ = 0;

    static void EyeIdleTimerCallback(void* arg);
    static void InfoHideTimerCallback(void* arg);
    void ApplyEyeShape();
    void SetEyesHidden(bool hidden);

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void ShowInfoText(const char* text, int duration_ms) override;
};

#endif // OLED_DISPLAY_H
