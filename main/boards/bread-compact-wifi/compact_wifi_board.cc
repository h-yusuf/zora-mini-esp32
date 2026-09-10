#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "CompactWifiBoard"

// Location used for the info ticker's weather line (open-meteo coordinates).
#ifndef INFO_TICKER_CITY
#define INFO_TICKER_CITY "Semarang"
#endif
#ifndef INFO_TICKER_LAT
#define INFO_TICKER_LAT "-6.99"
#endif
#ifndef INFO_TICKER_LON
#define INFO_TICKER_LON "110.42"
#endif

class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    uint8_t display_address_ = 0;  // 0 = no display detected on the bus
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;

    // Read the I2C pins as plain inputs with the internal pull-up on. Both
    // should read 1 when idle. A 0 means something is holding the line down:
    // a short to GND, or an unpowered device clamping it through its ESD diodes.
    void CheckDisplayBusIdleLevels() {
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = (1ULL << DISPLAY_SDA_PIN) | (1ULL << DISPLAY_SCL_PIN);
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        if (gpio_config(&cfg) != ESP_OK) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        int sda = gpio_get_level(DISPLAY_SDA_PIN);
        int scl = gpio_get_level(DISPLAY_SCL_PIN);
        ESP_LOGI(TAG, "I2C idle levels: SDA(GPIO%d)=%d SCL(GPIO%d)=%d",
                 (int)DISPLAY_SDA_PIN, sda, (int)DISPLAY_SCL_PIN, scl);
        if (sda == 0 || scl == 0) {
            ESP_LOGW(TAG, "I2C line held low - check for a short to GND, or a display "
                          "wired up without its 3V3 supply connected");
        }
    }

    void InitializeDisplayI2c() {
        CheckDisplayBusIdleLevels();

        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));

        // Probe the two addresses an SSD1306 can sit at. Talking to a panel
        // that is not there makes esp_lcd_panel_init() block forever, so the
        // result decides whether the display is brought up at all - the board
        // must still boot with no display wired.
        for (uint8_t addr : {(uint8_t)0x3C, (uint8_t)0x3D}) {
            if (i2c_master_probe(display_i2c_bus_, addr, 50) == ESP_OK) {
                ESP_LOGI(TAG, "Display found at 0x%02X", addr);
                display_address_ = addr;
                break;
            }
        }
        if (display_address_ == 0) {
            ESP_LOGW(TAG, "No display on SDA=%d SCL=%d - continuing without one",
                     (int)DISPLAY_SDA_PIN, (int)DISPLAY_SCL_PIN);
            // Leave the bus in a clean state after the failed probes.
            i2c_master_bus_reset(display_i2c_bus_);
        }
    }

    void InitializeSsd1306Display() {
        if (display_address_ == 0) {
            display_ = new NoDisplay();
            return;
        }

        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = display_address_,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // The SSD1306 needs its supply to settle before it will accept the init
        // sequence. Without this wait the very first command intermittently
        // fails with "io tx param SSD1306_CMD_SET_MULTIPLEX failed", which looks
        // exactly like a wiring fault.
        vTaskDelay(pdMS_TO_TICKS(100));

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "BOOT button clicked");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        // Manual market check without voice: double-click cycles USD/IDR, BTC/IDR, ETH/IDR.
        boot_button_.OnDoubleClick([this]() {
            static const char* pairs[] = {"usdidr", "btcidr", "ethidr"};
            static int index = 0;
            std::string pair = pairs[index % 3];
            index++;
            ESP_LOGI(TAG, "Manual market check: %s", pair.c_str());
            // Network I/O must not run in the button callback context.
            auto* arg = new std::string(pair);
            xTaskCreate(
                [](void* param) {
                    std::unique_ptr<std::string> p(static_cast<std::string*>(param));
                    auto& board = static_cast<CompactWifiBoard&>(Board::GetInstance());
                    board.ShowMarketPrice(*p);
                    vTaskDelete(nullptr);
                },
                "market_once", 6144, arg, 1, nullptr);
        });
    }

    // ---- Info ticker: rotate short info screens between the eyes ----------
    std::string weather_text_;
    std::string fx_text_;
    std::string btc_text_;

    // Keeps only printable ASCII so the built-in font can render it.
    static std::string SanitizeAscii(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (char c : in) {
            if (c == '\n' || ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f)) {
                out.push_back(c);
            }
        }
        // Collapse repeated spaces left behind by stripped glyphs.
        std::string collapsed;
        bool prev_space = false;
        for (char c : out) {
            bool is_space = (c == ' ');
            if (is_space && prev_space) continue;
            collapsed.push_back(c);
            prev_space = is_space;
        }
        return collapsed;
    }

    static bool HttpGet(const std::string& url, std::string& body_out) {
        auto network = Board::GetInstance().GetNetwork();
        if (network == nullptr) {
            return false;
        }
        auto http = network->CreateHttp(0);
        if (http == nullptr) {
            return false;
        }
        http->SetTimeout(10000);
        http->SetHeader("User-Agent", "curl/8.0");
        if (!http->Open("GET", url)) {
            return false;
        }
        int status = http->GetStatusCode();
        if (status != 200) {
            ESP_LOGW(TAG, "HTTP %d for %s", status, url.c_str());
            http->Close();
            return false;
        }
        body_out = http->ReadAll();
        http->Close();
        return !body_out.empty();
    }

    // WMO weather codes -> very short labels that fit a 128x64 screen.
    static const char* WeatherCodeText(int code) {
        switch (code) {
            case 0: return "Clear";
            case 1: return "Mostly clear";
            case 2: return "Partly cloudy";
            case 3: return "Overcast";
            case 45: case 48: return "Fog";
            case 51: case 53: case 55: return "Drizzle";
            case 61: case 63: return "Rain";
            case 65: return "Heavy rain";
            case 66: case 67: return "Freezing rain";
            case 71: case 73: case 75: case 77: return "Snow";
            case 80: case 81: return "Showers";
            case 82: return "Heavy showers";
            case 95: return "Thunderstorm";
            case 96: case 99: return "Storm + hail";
            default: return "";
        }
    }

    void RefreshWeather() {
        std::string body;
        if (!HttpGet("https://api.open-meteo.com/v1/forecast?latitude=" INFO_TICKER_LAT
                     "&longitude=" INFO_TICKER_LON "&current=temperature_2m,weather_code",
                     body)) {
            ESP_LOGW(TAG, "Weather fetch failed");
            return;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (root == nullptr) {
            return;
        }
        cJSON* current = cJSON_GetObjectItem(root, "current");
        cJSON* temp = current != nullptr ? cJSON_GetObjectItem(current, "temperature_2m") : nullptr;
        cJSON* code = current != nullptr ? cJSON_GetObjectItem(current, "weather_code") : nullptr;
        if (cJSON_IsNumber(temp)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s %.0fC\n%s", INFO_TICKER_CITY, temp->valuedouble,
                     cJSON_IsNumber(code) ? WeatherCodeText(code->valueint) : "");
            weather_text_ = SanitizeAscii(buf);
            ESP_LOGI(TAG, "Weather: %s", weather_text_.c_str());
        }
        cJSON_Delete(root);
    }

    // Group digits with a separator, e.g. 1620000000 -> "1.620.000.000".
    static std::string GroupDigits(double value, char sep, int decimals) {
        char raw[64];
        snprintf(raw, sizeof(raw), "%.*f", decimals, value);
        std::string s(raw);
        std::string frac;
        auto dot = s.find('.');
        if (dot != std::string::npos) {
            frac = s.substr(dot);  // includes the '.'
            s = s.substr(0, dot);
        }
        std::string grouped;
        int count = 0;
        for (int i = (int)s.size() - 1; i >= 0; i--) {
            grouped.insert(grouped.begin(), s[i]);
            if (++count % 3 == 0 && i > 0) {
                grouped.insert(grouped.begin(), sep);
            }
        }
        return grouped + frac;
    }

    // Fetch a live market price. Handles crypto and USD/IDR:
    //  *idr  -> Indodax public ticker (live Indonesian market, rupiah)
    //  *usd  -> Binance spot price against USDT
    // Returns false if the pair is unknown or the request fails.
    static bool FetchMarketPrice(const std::string& pair_in, std::string& display_out,
                                 std::string& spoken_out) {
        std::string pair;
        for (char c : pair_in) {
            if (isalnum((unsigned char)c)) {
                pair.push_back(tolower((unsigned char)c));
            }
        }
        if (pair.size() < 6) {
            return false;
        }

        std::string base, quote;
        if (pair.size() >= 6 && pair.compare(pair.size() - 3, 3, "idr") == 0) {
            base = pair.substr(0, pair.size() - 3);
            quote = "IDR";
            // Indodax quotes the dollar as USDT/IDR.
            std::string indodax_pair = (base == "usd") ? "usdtidr" : base + "idr";
            std::string body;
            if (!HttpGet("https://indodax.com/api/ticker/" + indodax_pair, body)) {
                return false;
            }
            cJSON* root = cJSON_Parse(body.c_str());
            if (root == nullptr) {
                return false;
            }
            cJSON* ticker = cJSON_GetObjectItem(root, "ticker");
            cJSON* last = ticker != nullptr ? cJSON_GetObjectItem(ticker, "last") : nullptr;
            bool ok = false;
            if (cJSON_IsString(last)) {
                double price = strtod(last->valuestring, nullptr);
                if (price > 0) {
                    std::string num = GroupDigits(price, '.', 0);
                    char buf[96];
                    // Uppercase base for display.
                    std::string up = base;
                    for (auto& c : up) c = toupper((unsigned char)c);
                    snprintf(buf, sizeof(buf), "%s/IDR\n%s", up.c_str(), num.c_str());
                    display_out = buf;
                    snprintf(buf, sizeof(buf), "%s/IDR = %s", up.c_str(), num.c_str());
                    spoken_out = buf;
                    ok = true;
                }
            }
            cJSON_Delete(root);
            return ok;
        }

        if (pair.size() >= 6 &&
            (pair.compare(pair.size() - 3, 3, "usd") == 0 ||
             pair.compare(pair.size() - 4, 4, "usdt") == 0)) {
            base = pair.compare(pair.size() - 4, 4, "usdt") == 0 ? pair.substr(0, pair.size() - 4)
                                                                 : pair.substr(0, pair.size() - 3);
            std::string up = base;
            for (auto& c : up) c = toupper((unsigned char)c);
            std::string body;
            if (!HttpGet("https://api.binance.com/api/v3/ticker/price?symbol=" + up + "USDT",
                         body)) {
                return false;
            }
            cJSON* root = cJSON_Parse(body.c_str());
            if (root == nullptr) {
                return false;
            }
            cJSON* price_item = cJSON_GetObjectItem(root, "price");
            bool ok = false;
            if (cJSON_IsString(price_item)) {
                double price = strtod(price_item->valuestring, nullptr);
                if (price > 0) {
                    std::string num = GroupDigits(price, ',', price < 1000 ? 2 : 0);
                    char buf[96];
                    snprintf(buf, sizeof(buf), "%s/USD\n%s", up.c_str(), num.c_str());
                    display_out = buf;
                    snprintf(buf, sizeof(buf), "%s/USD = %s", up.c_str(), num.c_str());
                    spoken_out = buf;
                    ok = true;
                }
            }
            cJSON_Delete(root);
            return ok;
        }

        return false;
    }

    void RefreshFx() {
        std::string display, spoken;
        if (FetchMarketPrice("usdidr", display, spoken)) {
            fx_text_ = display;
            ESP_LOGI(TAG, "FX: %s", spoken.c_str());
        } else {
            ESP_LOGW(TAG, "FX fetch failed");
        }
    }

    void RefreshBtc() {
        std::string display, spoken;
        if (FetchMarketPrice("btcidr", display, spoken)) {
            btc_text_ = display;
            ESP_LOGI(TAG, "BTC: %s", spoken.c_str());
        }
    }

    // Fetch a pair and put it on screen; also returns text for the assistant.
    std::string ShowMarketPrice(const std::string& pair) {
        std::string display, spoken;
        if (!FetchMarketPrice(pair, display, spoken)) {
            GetDisplay()->ShowInfoText("price\nunavailable", 4000);
            return "Could not fetch the price for " + pair;
        }
        GetDisplay()->ShowInfoText(display.c_str(), 8000);
        return spoken;
    }

    void InitializeMarketTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.market.get_price",
            "Show a live market price on the device screen and report it back.\n"
            "Use this whenever the user asks for the price of a currency or a crypto asset, "
            "for example \"usd idr\", \"harga bitcoin\", \"btc idr\", \"eth usd\".\n"
            "The `pair` argument is the trading pair without a separator, base first then quote, "
            "e.g. usdidr, btcidr, ethidr, solidr, btcusd, ethusd. "
            "Pairs ending in IDR use the live Indonesian market (Indodax); "
            "pairs ending in USD use Binance spot.",
            PropertyList({
                Property("pair", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto pair = properties["pair"].value<std::string>();
                return ShowMarketPrice(pair);
            });
    }

    // Read the mic directly for a couple of seconds and report the signal it
    // produces. Runs before the cloud is involved, so the microphone can be
    // validated even when activation or DNS is failing.
    void MicSelfTest() {
        auto codec = GetAudioCodec();
        codec->EnableInput(true);

        // The first reads after enabling the channel return the DMA buffers as
        // they were before the mic started clocking data in, i.e. all zeros.
        // Give the I2S peripheral a moment and throw those frames away,
        // otherwise the test reports "no signal" on a perfectly good mic.
        vTaskDelay(pdMS_TO_TICKS(200));
        {
            std::vector<int16_t> discard(codec->input_sample_rate() / 100 *
                                         codec->input_channels());
            for (int i = 0; i < 10; i++) {
                codec->InputData(discard);
            }
        }

        const int frames = 60;  // ~2 s at 30 ms per read
        int32_t worst_peak = 0;
        int64_t sum_abs = 0;
        int64_t total_samples = 0;
        int stuck_high = 0, stuck_zero = 0;

        std::vector<int16_t> data(codec->input_sample_rate() / 100 * codec->input_channels());
        for (int i = 0; i < frames; i++) {
            if (!codec->InputData(data) || data.empty()) {
                vTaskDelay(pdMS_TO_TICKS(30));
                continue;
            }
            int32_t peak = 0;
            bool all_ones = true, all_zero = true;
            for (int16_t s : data) {
                int32_t v = s < 0 ? -s : s;
                if (v > peak) peak = v;
                sum_abs += v;
                if (s != -1) all_ones = false;
                if (s != 0) all_zero = false;
            }
            total_samples += data.size();
            if (peak > worst_peak) worst_peak = peak;
            if (all_ones) stuck_high++;
            if (all_zero) stuck_zero++;
        }

        int32_t avg = total_samples > 0 ? (int32_t)(sum_abs / total_samples) : 0;
        ESP_LOGI(TAG, "Mic self test: peak=%d avg=%d frames_stuck_high=%d frames_all_zero=%d",
                 (int)worst_peak, (int)avg, stuck_high, stuck_zero);
        if (worst_peak <= 1) {
            ESP_LOGW(TAG, "Mic self test: no signal. The data line looks floating - "
                          "check that SD/WS/SCK are soldered and VDD is 3V3.");
        } else if (worst_peak >= 32767) {
            ESP_LOGW(TAG, "Mic self test: clipping at full scale. Lower the gain "
                          "(increase the shift in NoAudioCodec::Read).");
        } else {
            ESP_LOGI(TAG, "Mic self test: signal looks sane.");
        }
    }

    // Play a short 1 kHz tone through the amp/speaker. Runs before the cloud
    // is involved, so the amplifier wiring can be validated on its own.
    void SpeakerSelfTest() {
        auto codec = GetAudioCodec();
        codec->SetOutputVolume(70);
        codec->EnableOutput(true);

        const int sample_rate = codec->output_sample_rate();
        const int channels = codec->output_channels();
        const double freq = 1000.0;
        const double duration_s = 1.5;
        const int total_samples = (int)(sample_rate * duration_s);

        ESP_LOGI(TAG, "Speaker self test: playing %.0f Hz tone for %.1fs", freq, duration_s);
        const int chunk = sample_rate / 20;  // 50 ms per chunk
        std::vector<int16_t> data(chunk * channels);
        int played = 0;
        int sample_index = 0;
        while (played < total_samples) {
            int this_chunk = std::min(chunk, total_samples - played);
            data.resize(this_chunk * channels);
            for (int i = 0; i < this_chunk; i++) {
                double t = (double)(sample_index + i) / sample_rate;
                int16_t v = (int16_t)(12000.0 * sin(2.0 * M_PI * freq * t));
                for (int c = 0; c < channels; c++) {
                    data[i * channels + c] = v;
                }
            }
            codec->OutputData(data);
            played += this_chunk;
            sample_index += this_chunk;
        }
        ESP_LOGI(TAG, "Speaker self test done");
    }

    void InfoTickerLoop() {
        // Quick hardware check right after boot: sample the mic directly and
        // play a tone through the amp, so wiring can be confirmed without
        // waiting on WiFi/cloud.
        SpeakerSelfTest();
        MicSelfTest();

        // Give WiFi/activation time to settle before the first fetch.
        vTaskDelay(pdMS_TO_TICKS(20000));
        RefreshWeather();
        RefreshFx();
        RefreshBtc();
        int64_t last_weather_us = esp_timer_get_time();
        int64_t last_market_us = esp_timer_get_time();
        int slot = 0;

        while (true) {
            // Weather changes slowly; market prices are refreshed far more often.
            if (esp_timer_get_time() - last_weather_us > 600LL * 1000 * 1000) {
                last_weather_us = esp_timer_get_time();
                RefreshWeather();
            }
            if (esp_timer_get_time() - last_market_us > 60LL * 1000 * 1000) {
                last_market_us = esp_timer_get_time();
                RefreshFx();
                RefreshBtc();
            }

            std::string text;
            switch (slot % 5) {
                case 0: {
                    time_t now = time(nullptr);
                    struct tm tm_now;
                    localtime_r(&now, &tm_now);
                    char buf[48];
                    strftime(buf, sizeof(buf), "%H:%M\n%a %d %b", &tm_now);
                    text = buf;
                    break;
                }
                case 1:
                    text = weather_text_;
                    break;
                case 2:
                    text = fx_text_;
                    break;
                case 3:
                    text = btc_text_;
                    break;
                default: {
                    int64_t up_s = esp_timer_get_time() / 1000000;
                    char buf[48];
                    snprintf(buf, sizeof(buf), "Up %lldm %llds\nRAM %u KB", up_s / 60, up_s % 60,
                             (unsigned)(esp_get_free_heap_size() / 1024));
                    text = buf;
                    break;
                }
            }
            slot++;

            // Only interrupt the eyes when the device is not busy talking.
            auto state = Application::GetInstance().GetDeviceState();
            if (!text.empty() && state == kDeviceStateIdle) {
                GetDisplay()->ShowInfoText(text.c_str(), 4000);
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }

    void StartInfoTicker() {
        xTaskCreate(
            [](void* arg) {
                static_cast<CompactWifiBoard*>(arg)->InfoTickerLoop();
                vTaskDelete(nullptr);
            },
            "info_ticker", 6144, this, 1, nullptr);
    }

public:
    CompactWifiBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeMarketTools();
        StartInfoTicker();
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
