# ESP32-S3 SuperMini — Status Build (xiaozhi-esp32)

Branch ini dikhususkan untuk satu device. Cara build & flash:
[`SETUP.md`](../SETUP.md). Wiring lengkap: [`wiring-bread-compact-wifi.md`](wiring-bread-compact-wifi.md).

## Info Board

| Item | Detail |
|---|---|
| Chip | ESP32-S3 (QFN56), rev v0.2 |
| Flash | 4 MB embedded (XMC), QIO @ 80 MHz |
| PSRAM | 2 MB embedded, **Quad** (AP_3v3, gen 3) @ 80 MHz |
| GPIO ter-breakout | GPIO0-13 saja, plus 5V/GND/3V3/TX/RX |
| USB | native USB-Serial/JTAG (VID `0x303A`, PID `0x1001`), tanpa bridge CP210x/CH340 |
| Firmware | xiaozhi v2.4.2, ESP-IDF v6.1 |
| Board config | `bread-compact-wifi` |
| Backend | **Zora Bridge** (self-host), lewat `CONFIG_OTA_URL` di
  `main/Kconfig.projbuild` — bukan lagi `api.tenclass.net` |

## Yang Sudah Terintegrasi ✅

- **WiFi**: provisioning via AP (`Zora-XXXX` → `http://192.168.4.1`).
- **OTA check**: cek versi firmware & alamat server (MQTT/WebSocket) ke
  `CONFIG_OTA_URL` (Zora Bridge, path `/ota/check_version`) tiap boot.
- **Aktivasi device**: kode alfanumerik ditampilkan di layar (`ShowInfoText`)
  dan di-log (`Activation code: ...`) selain dibunyikan lewat speaker —
  dimasukkan manual ke dashboard Zora Bridge.
- **MQTT/WebSocket**: koneksi ke server percakapan sesuai respons OTA check.
- **Wake word**: model `wn9_nihaoxiaozhi_tts` ("你好小智") aktif, WebRTC VAD.
- **Mic INMP441** (GPIO4/5/6): self-test otomatis tiap boot, sebelum WiFi/cloud
  terlibat (`MicSelfTest()` di `compact_wifi_board.cc`).
- **Amplifier MAX98357A** (GPIO7/10/11): I2S digital langsung, tanpa DAC tambahan.
- **Display OLED SSD1306 128x64 (I2C, GPIO8/9)**:
  - Address `0x3C`/`0x3D` diprobe otomatis; kalau tidak ada yang menjawab,
    device tetap boot tanpa display (`NoDisplay`), tidak hang.
  - Color inverted (`invert_color(true)`) — mata nyala terang, background gelap.
  - UI custom: **RoboEyes-style fullscreen eyes** (bukan default status bar+jam+wifi+baterai):
    2 mata rounded-rect, blink tiap ~3 detik, idle saccade (lirik-lirik random) tiap ~2.5 detik.
  - **Ekspresi mata reaktif**: `SetEmotion()` dari server mengubah bentuk mata
    (senang = squint, sedih = droopy, kaget = melebar, mikir = agak menyempit, dll).
- **Info ticker** (`compact_wifi_board.cc`): tiap 10 detik layar menampilkan info
  bergantian selama 4 detik, lalu balik ke mata. Hanya jalan saat device idle
  (tidak menginterupsi percakapan). Isinya:
  - Jam + tanggal (waktu lokal device)
  - Cuaca via **open-meteo** (`api.open-meteo.com`, tanpa API key) — lokasi
    diatur lewat `INFO_TICKER_CITY` / `INFO_TICKER_LAT` / `INFO_TICKER_LON`
  - Kurs USD/IDR **realtime** via Indodax (`usdtidr`)
  - BTC/IDR **realtime** via Indodax
  - Uptime + free RAM
  - Cuaca refresh tiap 10 menit, harga pasar tiap 1 menit.
- **MCP tool `self.market.get_price`** — harga pasar realtime lewat perintah
  suara. LLM di server memanggil tool ini saat user menanyakan harga
  ("usd idr", "harga bitcoin", "eth usd"), device fetch lalu menampilkan di
  layar 8 detik dan mengembalikan teksnya agar dibacakan.
  - Argumen `pair`: pasangan tanpa pemisah, base lalu quote — `usdidr`,
    `btcidr`, `ethidr`, `solidr`, `btcusd`, `ethusd`, dst.
  - Pair berakhiran **IDR** → Indodax public ticker (pasar Indonesia,
    realtime). `usdidr` dipetakan ke `usdtidr`.
  - Pair berakhiran **USD** → Binance spot (`api.binance.com/api/v3/ticker/price`).
  - Angka diformat bergrup: IDR pakai titik (`1.402.584.000`), USD pakai koma.
- **Tes tanpa mic**: **double-click tombol BOOT** memutar `usdidr` → `btcidr` →
  `ethidr`. Fetch dijalankan di task terpisah supaya tidak memblokir callback
  tombol.
- **MCP tools terdaftar**: `self.market.get_price`, get_system_info, reboot,
  upgrade_firmware, screen (get_info/snapshot/preview_image/set_theme),
  assets download url.
- **Tombol**: hanya BOOT (GPIO0) — tidak ada LED, touch, volume, atau lamp
  di board ini (dihapus dari kode, bukan sekadar dimatikan).

## Riwayat Masalah yang Sudah Diperbaiki

1. Config PSRAM salah waktu bring-up (Octal → harus **Quad** untuk SuperMini)
   → boot-loop `PSRAM chip is not connected, or wrong PSRAM line mode`.
2. Race condition: notifikasi ketutup jam tiap 1 detik (clock tick) → di-fix
   dengan guard `notification_showing` di `UpdateStatusBar()`.
3. OLED awalnya di-drive sebagai 128x32 padahal fisiknya 128x64 → diperbaiki
   `CONFIG_OLED_SSD1306_128X64`.
4. Warna layar kebalik (mata gelap, background nyala) → fix dengan
   `esp_lcd_panel_invert_color(panel_, true)`.
5. **Bug bit-shift I2S mic** (`no_audio_codec.cc` `Read()`): kode aslinya
   `bit32_buffer[i] >> 12`. INMP441 kirim data 24-bit left-aligned di slot
   32-bit, jadi full-scale = ±2³¹; shift 12 menyisakan ~20 bit magnitude →
   **16x melebihi kapasitas int16** → tiap ada suara langsung ter-clamp ke
   `32767`. Akibatnya audio yang dikirim ke server berupa gelombang kotak
   rusak, VAD/WakeNet menolak, STT tidak pernah mengembalikan teks. Fix: `>> 16`.
6. `esp_lcd_panel_init()` ke display yang tidak terpasang **hang tanpa batas**
   (bukan gagal cepat) → seluruh boot mati sebelum sempat WiFi/self-test.
   Fix: probe address `0x3C`/`0x3D` dulu; kalau kosong, skip init panel sama
   sekali dan pakai `NoDisplay`.
7. Endpoint kurs `api.frankfurter.app`/`.dev` ternyata kurs referensi ECB
   (update sekali per hari kerja, bukan realtime) → diganti Indodax/Binance.
   `wttr.in` gagal TLS handshake di device ini (`mbedtls -0x0050`), diganti
   open-meteo.

Catatan: log `esp-tls-mbedtls: read error :-0x004C` / `SSL receive failed: -76`
muncul di setiap request HTTPS ini. Itu server menutup koneksi TLS tanpa
close-notify setelah body terkirim penuh — tidak berbahaya, body tetap
lengkap dan berhasil di-parse.

## Catatan Debug yang Masih Aktif

Log debug berikut sengaja dibiarkan untuk verifikasi hardware (throttled,
tidak spam):
- `AudioService: Mic level: peak=.. avg=..` (~3 detik sekali).
- `NoAudioCodec: raw i2s: ........` (~10 detik sekali) — nilai mentah 32-bit
  dari jalur I2S.
- `CompactWifiBoard: BOOT button clicked` — konfirmasi tombol terbaca.
- `CompactWifiBoard: Mic self test: ...` — sekali tiap boot, sebelum cloud.
- `CompactWifiBoard: I2C idle levels: ...` — level SDA/SCL saat idle, sebelum
  bus di-init.

**Cara baca:** raw i2s harus menunjukkan nilai bervariasi (misal `fffff2a0`,
`0001a3c0`). Kalau isinya `00000000`/`00000001` terus = jalur data
floating/tidak nyambung. Kalau lompat antara nol dan full-scale = juga
floating. Klik BOOT lalu langsung bicara; kalau berhasil akan muncul baris
`>> <teks hasil transkrip>`.
