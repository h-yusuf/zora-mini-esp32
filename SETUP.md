# SETUP — ESP32-S3 DevKit N16R8

Branch ini dikhususkan untuk satu board: **ESP32-S3 DevKit N16R8** (16 MB
flash, 8 MB PSRAM Octal, full GPIO breakout), dengan mic INMP441, OLED SSD1306
128x64, dan amplifier MAX98357A.

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
dipakai — gunakan `activate_idf_v6.1.sh`.

---

## 2. Build & flash

Tidak ada pilihan board di branch ini — semua sudah default ke N16R8 lewat
`sdkconfig.defaults.esp32s3`, jadi tinggal:

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py set-target esp32s3   # sekali saja, atau setelah git clean
idf.py build flash monitor
```

Board ini punya **2 port USB-C**: pakai yang berlabel **`COM`/`UART`** buat
flash & monitor, bukan yang berlabel `USB` (native — auto-reset gak jalan di
situ, hasilnya `Failed to connect: No serial data received`).

Kalau port tidak otomatis kedeteksi: `idf.py -p /dev/cu.usbmodem101 flash monitor`.
Keluar dari monitor: `Ctrl+]`.

`set-target esp32s3` otomatis memilih board `bread-compact-wifi`, flash 16 MB,
partition `partitions/v2/16m.csv`, PSRAM Octal, dan panel `SSD1306 128x64` —
semuanya lewat `sdkconfig.defaults.esp32s3`, tanpa menuconfig manual.

---

## 3. Menjalankan pertama kali

1. **Flash**, lalu buka monitor.
2. **Provisioning WiFi** — device membuat AP sendiri `Zora-XXXX`. Sambungkan
   laptop/HP ke AP itu, buka `http://192.168.4.1`, masukkan SSID + password.
   Matikan data seluler kalau lewat HP, kalau tidak browser akan mencari internet
   ke jalur lain.
3. **Aktivasi** — device menampilkan kode 6 digit. Tanpa layar terpasang, ambil
   dari log monitor; barisnya seperti:
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

Tiap boot menjalankan self-test mic & speaker otomatis, jadi tidak perlu menunggu cloud:

| Log | Arti |
|---|---|
| `Display found at 0x3C` + `Adding OLED display` | OLED OK |
| `No display on SDA=.. SCL=..` | tidak ada yang menjawab di bus — cek 3V3/GND & wiring; device tetap boot |
| `Mic self test: signal looks sane` | mic OK |
| `frames_all_zero=60` | mic tidak dapat supply / kontak, jalur data mati |
| `frames_stuck_high=...` | jalur data mengambang |
| `peak=32767` | sinyal clipping, gain terlalu tinggi |
| `Speaker self test: playing 1000 Hz tone` | nada tes speaker diputar ~1.5 detik |

Kalau speaker senyap, tarik pin `SD` MAX98357A ke 3V3 dan pastikan `VIN` di 5 V.

---

## 5. Fitur tambahan di repo ini

- **Mata robot fullscreen** di OLED 128x64 (gaya RoboEyes): dua mata rounded-rect,
  berkedip, gerak lirik acak, dan **berubah bentuk mengikuti emosi** dari server
  (senang menyipit, sedih turun, kaget membesar). Status bar/jam/wifi dihilangkan.
  Implementasi: `main/display/oled_display.cc` (`SetupUI_128x64`, `SetEmotion`).
- **Info ticker** — tiap 10 detik menampilkan info bergantian: jam+tanggal,
  cuaca (open-meteo), USD/IDR, BTC/IDR, uptime+RAM. Cuaca refresh 10 menit,
  harga tiap 1 menit. Lokasi cuaca diatur lewat `INFO_TICKER_CITY/LAT/LON`
  di `compact_wifi_board.cc`.
- **MCP tool `self.market.get_price`** — harga pasar realtime lewat suara.
  Bilang "usd idr" / "harga bitcoin" / "eth usd", LLM server memanggil tool ini,
  device fetch lalu menampilkannya 8 detik dan membacakannya.
  Pair `*IDR` → Indodax (pasar Indonesia, realtime), `*USD` → Binance spot.
  **Double-click BOOT** memutar `usdidr` → `btcidr` → `ethidr` untuk tes tanpa suara.

Board ini disederhanakan ke 3 komponen fisik (mic, LCD, ampli) plus tombol BOOT
bawaan — tidak ada LED, tombol touch/volume, atau lamp, walau devkit N16R8
sebenarnya punya pin untuk itu (GPIO48/47/40/39/18).

---

## 6. Troubleshooting

Semua di bawah ini benar-benar terjadi saat setup, dengan gejala persisnya.

**Port serial tidak muncul di `/dev/cu.*`**
macOS menahan aksesori USB baru. Approve pop-up "Allow accessory to connect",
atau System Settings → Privacy & Security → Accessories. Device terlihat di
`ioreg -p IOUSB` tapi tanpa node `/dev/cu.*` = izin belum diberikan.

**`Failed to connect to ESP32-S3: No serial data received`**
Kabel tertancap di port **USB** (native) bukan **COM**. Auto-reset DTR/RTS hanya
ada di jalur COM. Pindahkan kabel.

**`Could not exclusively lock port ... Resource temporarily unavailable`**
Monitor masih terbuka dan memegang port. Tutup dengan `Ctrl+]` sebelum flash.

**Board boot-loop, `PSRAM chip is not connected, or wrong PSRAM line mode`**
Config PSRAM salah (Quad vs Octal). N16R8 pakai **Octal** — cek
`CONFIG_SPIRAM_MODE_OCT=y` di sdkconfig.

**`generated_assets.bin will not fit in ... bytes of flash`**
Config flash size salah untuk board ini. N16R8 butuh 16 MB /
`partitions/v2/16m.csv`, bukan profil 4 MB.

**Layar menyala tapi warnanya kebalik**
`esp_lcd_panel_invert_color(panel_, true)` di `compact_wifi_board.cc`.

**Tampilan OLED kepotong / posisi aneh**
Tipe panel salah. Pastikan `CONFIG_OLED_SSD1306_128X64=y` (default di
`sdkconfig.defaults.esp32s3`), bukan `128X32`.

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

**`getaddrinfo() returns 202` saat menghubungi server**
DNS gagal, biasanya transient beberapa detik pertama setelah boot; device retry
otomatis tiap 10 detik. Kalau menetap, jaringannya memang tanpa DNS/internet.

**SSID tampil sebagai `???` di log**
Kosmetik saja — SSID memuat karakter non-ASCII yang tidak bisa dirender console.

---

## 7. Command penting & cara baca log

### Command sehari-hari

```bash
# Aktifin ESP-IDF (sekali per terminal baru)
source ~/.espressif/tools/activate_idf_v6.1.sh

# Build doang, gak flash
idf.py build

# Flash + langsung buka monitor (paling sering dipakai)
idf.py -p /dev/cu.usbmodem101 flash monitor

# Cuma buka monitor tanpa flash ulang
idf.py -p /dev/cu.usbmodem101 monitor

# Keluar dari monitor
Ctrl+]

# Cek port yang lagi nyolok
ls /dev/cu.usbmodem*

# Bersihin total & mulai dari nol (kalau config aneh/nyangkut)
rm -rf build sdkconfig
idf.py set-target esp32s3
idf.py build flash monitor
```

Kalau muncul error port "busy"/"Resource temporarily unavailable" pas flash,
berarti ada monitor lain masih nyekel port itu — tutup dulu (`Ctrl+]`) baru
flash lagi. Port cuma bisa dipegang **satu proses** dalam satu waktu.

### Cara baca log monitor

Setiap baris log formatnya: `LEVEL (waktu_ms) TAG: pesan`.

| Level | Arti |
|---|---|
| `I` (Info) | Normal, informasi jalannya sistem |
| `W` (Warning) | Ada yang gak ideal tapi device tetap jalan |
| `E` (Error) | Ada yang gagal — perhatikan, tapi cek apakah device tetap recover |

Baris `TAG` yang paling sering muncul dan artinya:

| TAG | Fungsinya |
|---|---|
| `CompactWifiBoard` | Kode board ini — hasil self-test mic/speaker, I2C, log tombol BOOT |
| `StateMachine` | Perpindahan state device (`starting` → `wifi_configuring` → `activating` → `idle` → `listening` → `speaking`) |
| `WifiManager` / `WifiStation` | Status koneksi WiFi |
| `MQTT` | Koneksi ke server percakapan |
| `Application` | Alur percakapan tingkat tinggi (alert, error, aktivasi) |
| `AudioService` / `NoAudioCodec` | Level sinyal mic mentah (debug) |
| `Ota` | Cek versi firmware ke server |

Baris kunci yang perlu dicari waktu troubleshooting:

```
State: ... -> idle              # device siap, gak ada masalah pending
State: ... -> listening          # abis klik BOOT, lagi dengerin
>> <teks>                        # hasil transkrip suara kamu (STT) — mic sampai ke server
<< <teks>                        # jawaban asisten (sebelum dibacakan)
Mic self test: signal looks sane # mic hidup & wiring OK (dicek otomatis tiap boot)
Display found at 0x3C            # OLED kedetek
Speaker self test: playing ...   # nada tes speaker jalan pas boot
Activation done                  # device udah ke-link/aktif, siap pakai
```

Kalau state balik ke `idle` sendiri tanpa pernah muncul `>>`, artinya sesi
ditutup sebelum server sempat transkrip — biasanya karena kelamaan diem
setelah klik BOOT, bukan karena mic rusak.
