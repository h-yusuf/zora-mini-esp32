# SETUP — ESP32-S3 (SuperMini & DevKit N16R8)

Catatan setup untuk dua modul ESP32-S3 yang dipakai di sini, keduanya memakai
board config `bread-compact-wifi` dengan mic INMP441, OLED SSD1306 128x64, dan
amplifier MAX98357A.

- Wiring detail: [`docs/wiring-bread-compact-wifi.md`](docs/wiring-bread-compact-wifi.md)
- Status & catatan hardware: [`docs/esp32-s3-status.md`](docs/esp32-s3-status.md)

---

## 1. Prasyarat (macOS)

```bash
brew install cmake dfu-util
```

ESP-IDF v6.1 dipasang lewat ESP-IDF Installation Manager (EIM) dari extension
VSCode: **ESP-IDF: Open ESP-IDF Installation Manager** → *Easy Installation*.
EIM akan menolak jalan kalau `cmake` / `dfu-util` belum ada.

Hasil instalasi (layout EIM, bukan layout klasik):

| Item | Path |
|---|---|
| ESP-IDF | `~/.espressif/v6.1/esp-idf` |
| Tools | `~/.espressif/tools` |
| Python venv | `~/.espressif/tools/python/v6.1/venv` |
| Activation | `~/.espressif/tools/activate_idf_v6.1.sh` |

Karena venv-nya di luar lokasi standar, `$IDF_PATH/export.sh` **tidak** bisa
dipakai — gunakan `activate_idf_v6.1.sh`. Script `scripts/xz.sh` sudah
menanganinya otomatis.

---

## 2. Build & flash

Board dipilih lewat env `XZ_BOARD`, tidak perlu mengedit `sdkconfig` manual:

```bash
XZ_BOARD=supermini ./scripts/xz.sh flash monitor
XZ_BOARD=n16r8     ./scripts/xz.sh flash
XZ_BOARD=supermini ./scripts/xz.sh menuconfig
```

Semua argumen diteruskan ke `idf.py` (`build`, `monitor`, `reconfigure`,
`fullclean`, dst). Keluar dari monitor: `Ctrl+]`.

Kalau dua board tersambung sekaligus, sebutkan port-nya:

```bash
XZ_BOARD=supermini XZ_PORT=/dev/cu.usbmodem101 ./scripts/xz.sh flash
XZ_BOARD=n16r8 XZ_PORT=/dev/cu.usbmodem5C930658041 ./scripts/xz.sh flash
```

Tiap board punya `sdkconfig.<board>` dan `build/<board>` sendiri, jadi
bergantian board tidak menimpa config board lain dan tidak memicu rebuild penuh.
Profil tersimpan di `sdkconfig.board.<nama>` dan ditumpuk di atas
`sdkconfig.defaults` + `sdkconfig.defaults.esp32s3`.

### Perbedaan dua board

| | SuperMini | DevKit N16R8 |
|---|---|---|
| Flash | 4 MB, `partitions/v2/4m.csv` | 16 MB, `partitions/v2/16m.csv` |
| PSRAM | 2 MB **Quad** | 8 MB **Octal** |
| Display I2C | SDA 8 / SCL 9 | SDA 41 / SCL 42 |
| Speaker BCLK / LRCK | 10 / 11 | 15 / 16 |
| Lamp / LED tes | 13 | 18 |
| LED / touch / volume | tidak terjangkau | 48 / 47 / 40-39 |
| USB | native (`cu.usbmodem101`) | pakai port **COM**, bukan port `USB` |

Header SuperMini hanya mengeluarkan **GPIO0-13**, jadi pin di luar rentang itu
tidak bisa dipakai. Mic tetap di GPIO4/5/6 dan speaker DOUT di GPIO7 karena
sudah berada dalam rentang tersebut.

Mode PSRAM **harus** cocok. Kalau SuperMini diflash dengan config Octal, board
langsung boot-loop: `PSRAM chip is not connected, or wrong PSRAM line mode`.

Pin display/speaker dipilih compile-time lewat Kconfig
`BREAD_COMPACT_WIFI_VARIANT` di `main/Kconfig.projbuild`, dibaca oleh
`main/boards/bread-compact-wifi/config.h`.

---

## 3. Menjalankan pertama kali

1. **Flash**, lalu buka monitor.
2. **Provisioning WiFi** — device membuat AP sendiri `Xiaozhi-XXXX`. Sambungkan
   laptop/HP ke AP itu, buka `http://192.168.4.1`, masukkan SSID + password.
   Matikan data seluler kalau lewat HP, kalau tidak browser akan mencari internet
   ke jalur lain.
3. **Aktivasi** — device menampilkan kode 6 digit. Tanpa layar, ambil dari log
   monitor; barisnya seperti:
   ```
   Application: Alert [link] 激活设备: xiaozhi.me
   403937
   ```
   Buka [xiaozhi.me](https://xiaozhi.me) → **Add Device** → masukkan kode.
4. **Reset device** setelah ter-link supaya keluar dari loop `Activating`.
5. Setelah `Activation done` dan state `activating -> idle`, device siap.

Percakapan: klik tombol **BOOT sekali**, lalu **langsung** bicara tanpa jeda.
Tombol itu toggle — klik kedua menutup sesi, jadi jangan diklik dua kali. Sesi
juga menutup sendiri kalau tidak ada suara terdeteksi.

Di log, `>>` adalah hasil transkrip suara kamu (STT) dan `<<` balasan asisten.

---

## 4. Verifikasi hardware

Tiap boot menjalankan self-test otomatis, jadi tidak perlu menunggu cloud:

| Log | Arti |
|---|---|
| `Display found at 0x3C` + `Adding OLED display` | OLED OK |
| `No display on SDA=.. SCL=..` | tidak ada yang menjawab di bus — cek 3V3/GND & wiring; device tetap boot |
| `Lamp self test: blinking GPIO..` | LED lamp dikedipkan 5x |
| `Mic self test: signal looks sane` | mic OK |
| `frames_all_zero=60` | mic tidak dapat supply / kontak, jalur data mati |
| `frames_stuck_high=...` | jalur data mengambang |
| `peak=32767` | sinyal clipping, gain terlalu tinggi |
| `Self test skipped: LED not initialized` | LED WS2812 dimatikan (normal di SuperMini) |

Speaker: device memainkan nada sukses setelah aktivasi. Kalau senyap, tarik pin
`SD` MAX98357A ke 3V3 dan pastikan `VIN` di 5 V.

---

## 5. Fitur tambahan di repo ini

- **Mata robot fullscreen** di OLED 128x64 (gaya RoboEyes): dua mata rounded-rect,
  berkedip, gerak lirik acak, dan **berubah bentuk mengikuti emosi** dari server
  (senang menyipit, sedih turun, kaget membesar). Status bar/jam/wifi dihilangkan.
  Implementasi: `main/display/oled_display.cc` (`SetupUI_128x64`, `SetEmotion`).
- **Info ticker** — tiap 10 detik menampilkan info bergantian: jam+tanggal,
  cuaca (open-meteo), USD/IDR, BTC/IDR, uptime+RAM. Cuaca refresh 10 menit,
  harga tiap 1 menit. Lokasi cuaca diatur lewat `INFO_TICKER_CITY/LAT/LON`.
- **MCP tool `self.market.get_price`** — harga pasar realtime lewat suara.
  Bilang "usd idr" / "harga bitcoin" / "eth usd", LLM server memanggil tool ini,
  device fetch lalu menampilkannya 8 detik dan membacakannya.
  Pair `*IDR` → Indodax (pasar Indonesia, realtime), `*USD` → Binance spot.
  **Double-click BOOT** memutar `usdidr` → `btcidr` → `ethidr` untuk tes tanpa suara.

---

## 6. Troubleshooting

Semua di bawah ini benar-benar terjadi saat setup, dengan gejala persisnya.

**Port serial tidak muncul di `/dev/cu.*`**
macOS menahan aksesori USB baru. Approve pop-up "Allow accessory to connect",
atau System Settings → Privacy & Security → Accessories. Device terlihat di
`ioreg -p IOUSB` tapi tanpa node `/dev/cu.*` = izin belum diberikan.

**`Failed to connect to ESP32-S3: No serial data received` (N16R8)**
Kabel tertancap di port **USB** (native) bukan **COM**. Auto-reset DTR/RTS hanya
ada di jalur COM. Pindahkan kabel.

**`Could not exclusively lock port ... Resource temporarily unavailable`**
Monitor masih terbuka dan memegang port. Tutup dengan `Ctrl+]` sebelum flash.

**Board boot-loop, `PSRAM chip is not connected, or wrong PSRAM line mode`**
Profil board salah (Octal vs Quad). Pakai `XZ_BOARD` yang benar.

**`generated_assets.bin will not fit in 4194304 bytes of flash`**
Config 16 MB dipakai untuk board 4 MB. Sama, salah profil.

**Layar menyala tapi warnanya kebalik**
`esp_lcd_panel_invert_color(panel_, true)` di `compact_wifi_board.cc`.

**Tampilan OLED kepotong / posisi aneh**
Tipe panel salah. Set `CONFIG_OLED_SSD1306_128X64` (bukan `128X32`).

**Notifikasi teks muncul sekilas lalu hilang**
Clock tick memanggil `UpdateStatusBar()` tiap detik dan `SetStatus()`
menyembunyikan notifikasi. Sudah diperbaiki dengan guard `notification_showing`
di `lvgl_display.cc`.

**`Mic level` melompat antara 0/1 dan tepat 32767 tanpa nilai tengah**
Skala 24-bit → int16 salah. Upstream memakai `>> 12` yang membuat sinyal wajar
langsung saturasi 16x; sudah diganti `>> 16` di `no_audio_codec.cc`. Kalau suara
terlalu pelan untuk STT, kecilkan shift ke 15 atau 14.

**Mic dan OLED mati bersamaan, hilang-muncul antar boot**
Rail 3V3/GND breadboard kontaknya tidak solid. Cek jumper power ke rail, ukur
3.3 V langsung di pad `VDD` mic dan `VCC` OLED, dan pastikan kedua rail
breadboard tersambung (banyak breadboard rail-nya terpotong di tengah).

**Mic terbaca `raw i2s: 00000000` atau `ffffffff` konstan**
Jalur data mengambang. Modul INMP441 bulat memakai pad setengah lubang
(castellated) — pin header **harus disolder**, ditekan saja tidak cukup.

**Mic mengirim data tapi server tidak pernah mengirim `>>`**
Cek `L/R` mic harus ke **GND** (firmware membaca slot LEFT). Kalau ke VDD, mic
mengirim di slot kanan dan yang terbaca nol.

**Speaker senyap padahal wiring benar**
`SD_MODE` MAX98357A menentukan channel: >1.4 V = kiri (yang dipakai), resistor ke
GND = kanan → senyap. Biarkan mengambang atau tarik ke 3V3.

**Amplifier analog (PAM8403 / HW-104) tidak bisa dipakai**
Modul itu butuh input analog, sedangkan ESP32-S3 **tidak punya DAC** (berbeda
dari ESP32 classic). Pakai MAX98357A yang menerima I2S digital langsung.

**TWS Bluetooth tidak bisa**
ESP32-S3 hanya punya BLE, tanpa Bluetooth Classic, jadi A2DP/HFP tidak tersedia.

**`getaddrinfo() returns 202` saat menghubungi `api.tenclass.net`**
DNS gagal, biasanya transient beberapa detik pertama setelah boot; device retry
otomatis tiap 10 detik. Kalau menetap, jaringannya memang tanpa DNS/internet.

**SSID tampil sebagai `???` di log**
Kosmetik saja — SSID memuat karakter non-ASCII yang tidak bisa dirender console.
