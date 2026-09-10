#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7

// On the SuperMini only GPIO0-13 are wired out to the header, so the speaker
// clocks cannot stay on the DevKit's GPIO15/16.
#ifdef CONFIG_BREAD_VARIANT_SUPERMINI
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_10
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_11
#else
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
#endif

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BOOT_BUTTON_GPIO        GPIO_NUM_0

// These sit outside GPIO0-13, so they are unreachable on the SuperMini header.
// GPIO_NUM_NC is accepted by both Button and SingleLed and simply disables them.
#ifdef CONFIG_BREAD_VARIANT_SUPERMINI
#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC
#else
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39
#endif

// Display I2C pins depend on the module this board is wired on:
// the SuperMini only breaks out GPIO0-13, the DevKit exposes the full range.
#ifdef CONFIG_BREAD_VARIANT_SUPERMINI
#define DISPLAY_SDA_PIN GPIO_NUM_8
#define DISPLAY_SCL_PIN GPIO_NUM_9
#else
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#endif
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#elif CONFIG_OLED_SH1106_128X64
#define DISPLAY_HEIGHT  64
#define SH1106
#else
#error "OLED display type is not selected"
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true


// A MCP Test: Control a lamp
#ifdef CONFIG_BREAD_VARIANT_SUPERMINI
#define LAMP_GPIO GPIO_NUM_13
#else
#define LAMP_GPIO GPIO_NUM_18
#endif

#endif // _BOARD_CONFIG_H_
