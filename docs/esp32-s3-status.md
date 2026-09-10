# ESP32-S3 — Status Build (xiaozhi-esp32)

## Board Switch (pilih target lewat env)

Dua modul dipakai bergantian dengan board config yang sama (`bread-compact-wifi`).
Profil per-board ada di `sdkconfig.board.<nama>` dan dipilih lewat `XZ_BOARD`:

```bash
XZ_BOARD=supermini ./scripts/xz.sh flash monitor
XZ_BOARD=n16r8     ./scripts/xz.sh build
XZ_BOARD=n16r8 XZ_PORT=/dev/cu.usbmodem5C930658041 ./scripts/xz.sh flash
XZ_BOARD=supermini ./scripts/xz.sh menuconfig
```

- Tiap board punya `sdkconfig.<board>` dan `build/<board>` sendiri → ganti board
  tidak menimpa config board lain dan tidak memicu rebuild penuh.
- `XZ_PORT` opsional: kalau cuma satu port serial terpasang, script mendeteksi sendiri.
- Argumen apa pun diteruskan ke `idf.py` (build, flash, monitor, menuconfig, ...).
- ESP-IDF diaktifkan otomatis kalau `idf.py` belum ada di PATH.

| Item | SuperMini | DevKit N16R8 |
|---|---|---|
| Profil | `sdkconfig.board.supermini` | `sdkconfig.board.n16r8` |
| Flash | 4 MB (`partitions/v2/4m.csv`) | 16 MB (`partitions/v2/16m.csv`) |
| PSRAM | 2 MB **Quad** | 8 MB **Octal** |
| Display I2C | SDA 8 / SCL 9 | SDA 41 / SCL 42 |
| Port | native USB (`cu.usbmodem101`) | port **COM**, bukan port `USB` |
| Device UUID | `86ee87c4-1365-4561-85c2-a6fb6e83eb2c` | `c88390a8-2436-4f42-bb34-0ceb2a2a13f3` |

Wiring lengkap mic / amplifier / OLED: [`wiring-bread-compact-wifi.md`](wiring-bread-compact-wifi.md)

Pin display dipilih compile-time lewat Kconfig `BREAD_COMPACT_WIFI_VARIANT`
(`CONFIG_BREAD_VARIANT_SUPERMINI` / `CONFIG_BREAD_VARIANT_DEVKIT`), dibaca di
`main/boards/bread-compact-wifi/config.h`. SuperMini hanya membreak-out GPIO0-13,
jadi tidak bisa memakai GPIO41/42.

## Info Umum

| Item | Detail |
|---|---|
| Chip | ESP32-S3, rev v0.2 |
| Firmware | xiaozhi v2.4.2, ESP-IDF v6.1 |
| Board config | `bread-compact-wifi` |

## Yang Sudah Terintegrasi ✅

- **WiFi**: provisioning via AP (`Xiaozhi-XXXX` → `http://192.168.4.1`), konek ke jaringan rumah/kost.
- **Aktivasi device**: linked ke akun xiaozhi.me (kode aktivasi via serial log).
- **OTA check**: cek versi firmware ke `api.tenclass.net` tiap boot.
- **MQTT**: koneksi ke server xiaozhi buat komunikasi voice assistant.
- **Wake word**: model `wn9_nihaoxiaozhi_tts` ("你好小智") aktif, WebRTC VAD.
- **Display OLED SSD1306 128x64 (I2C)**:
  - Pin: SDA=GPIO41, SCL=GPIO42, address `0x3C`
  - Color inverted (`invert_color(true)`) — mata nyala terang, background gelap
  - UI custom: **RoboEyes-style fullscreen eyes** (bukan default status bar+jam+wifi+baterai)
    - 2 mata rounded-rect, blink tiap ~3 detik
    - Idle saccade (lirik-lirik random posisi) tiap ~2.5 detik
  - Custom notification: pesan sambutan custom pernah ditampilkan (`ShowNotification`)
- **MCP tools terdaftar**: lamp control, volume, get_system_info, reboot, upgrade_firmware, screen (get_info/snapshot/preview_image/set_theme), assets download url.
- **Tombol**: BOOT (GPIO0), Touch (GPIO47, opsional — belum kepasang), Volume Up/Down (GPIO40/39).
- **LED bawaan**: GPIO48.

- **Ekspresi mata reaktif**: `SetEmotion()` dari server mengubah bentuk mata (senang = squint, sedih = droopy, kaget = melebar, mikir = agak menyempit, dll).
- **Info ticker** (`compact_wifi_board.cc`): tiap 10 detik layar menampilkan info bergantian selama 4 detik, lalu balik ke mata. Hanya jalan saat device idle (tidak menginterupsi percakapan). Isinya:
  - Jam + tanggal (waktu lokal device)
  - Cuaca via **open-meteo** (`api.open-meteo.com`, tanpa API key) — lokasi diatur lewat `INFO_TICKER_CITY` / `INFO_TICKER_LAT` / `INFO_TICKER_LON`
  - Kurs USD/IDR **realtime** via Indodax (`usdtidr`)
  - BTC/IDR **realtime** via Indodax
  - Uptime + free RAM
  - Cuaca refresh tiap 10 menit, harga pasar tiap 1 menit.
- **MCP tool `self.market.get_price`** — harga pasar realtime lewat perintah suara. LLM di server memanggil tool ini saat user menanyakan harga ("usd idr", "harga bitcoin", "eth usd"), device fetch lalu menampilkan di layar 8 detik dan mengembalikan teksnya agar dibacakan.
  - Argumen `pair`: pasangan tanpa pemisah, base lalu quote — `usdidr`, `btcidr`, `ethidr`, `solidr`, `btcusd`, `ethusd`, dst.
  - Pair berakhiran **IDR** → Indodax public ticker (pasar Indonesia, realtime). `usdidr` dipetakan ke `usdtidr`.
  - Pair berakhiran **USD** → Binance spot (`api.binance.com/api/v3/ticker/price`).
  - Angka diformat bergrup: IDR pakai titik (`1.402.584.000`), USD pakai koma.
- **Tes tanpa mic**: **double-click tombol BOOT** memutar `usdidr` → `btcidr` → `ethidr`. Fetch dijalankan di task terpisah supaya tidak memblokir callback tombol.

## Belum / Pending ⏳

- **Mic + Speaker + Amplifier**: fisik ada tapi **belum disolder** ke board. Perlu konfirmasi tipe:
  - Mic harus **I2S digital** (misal INMP441/SPH0645) — bukan mic analog biasa.
  - Amplifier harus **I2S amp** (misal MAX98357A) — bukan analog amp (PAM8403 dkk butuh DAC tambahan).
  - Pin config sudah disiapkan (`AUDIO_I2S_METHOD_SIMPLEX`):
    - Mic: WS=GPIO4, SCK=GPIO5, DIN=GPIO6
    - Speaker: DOUT=GPIO7, BCLK=GPIO15, LRCK=GPIO16
- **Chat text / jam / wifi icon**: sengaja dihilangkan dari layar (fullscreen eyes only) — kalau nanti mau tampilan info balik, perlu ubah lagi `SetupUI_128x64()`.

## Riwayat Masalah yang Sudah Diperbaiki

1. USB port salah (native USB vs COM) → auto-reset gak jalan di port USB, harus pindah ke COM.
2. Flash size & PSRAM mode salah waktu ganti board (4MB/Quad → 16MB/Octal untuk N16R8).
3. Race condition: notifikasi ketutup jam tiap 1 detik (clock tick) → di-fix dengan guard `notification_showing` di `UpdateStatusBar()`.
4. OLED awalnya di-drive sebagai 128x32 padahal fisiknya 128x64 → diperbaiki `CONFIG_OLED_SSD1306_128X64`.
5. Warna layar kebalik (mata gelap, background nyala) → fix dengan `esp_lcd_panel_invert_color(panel_, true)`.
6. **Bug bit-shift I2S mic** (`no_audio_codec.cc` `Read()`): kode aslinya `bit32_buffer[i] >> 12`. INMP441 kirim data 24-bit left-aligned di slot 32-bit, jadi full-scale = ±2³¹; shift 12 menyisakan ~20 bit magnitude → **16x melebihi kapasitas int16** → tiap ada suara langsung ter-clamp ke `32767`. Akibatnya audio yang dikirim ke server berupa gelombang kotak rusak, VAD/WakeNet menolak, STT tidak pernah mengembalikan teks. Fix: `>> 16`.
7. Endpoint kurs `api.frankfurter.app` sudah pindah ke `.dev`, dan datanya ternyata kurs referensi ECB (update sekali per hari kerja, bukan realtime) → diganti Indodax/Binance. `wttr.in` gagal TLS handshake di device ini (`mbedtls -0x0050`), diganti open-meteo.

Catatan: log `esp-tls-mbedtls: read error :-0x004C` / `SSL receive failed: -76` muncul di setiap request HTTPS ini. Itu server menutup koneksi TLS tanpa close-notify setelah body terkirim penuh — tidak berbahaya, body tetap lengkap dan berhasil di-parse.

## Catatan Debug yang Masih Aktif

Tiga log debug sengaja dibiarkan untuk verifikasi mic setelah disolder (semuanya throttled, tidak spam):
- `AudioService: Mic level: peak=.. avg=..` (~3 detik sekali) — level sinyal mic.
- `NoAudioCodec: raw i2s: ........` (~10 detik sekali) — nilai mentah 32-bit dari jalur I2S.
- `CompactWifiBoard: BOOT button clicked` — konfirmasi tombol terbaca.

**Cara baca hasilnya setelah mic disolder:** raw i2s harus menunjukkan nilai bervariasi (misal `fffff2a0`, `0001a3c0`). Kalau isinya `00000000`/`00000001` terus = jalur data floating/tidak nyambung. Kalau lompat antara nol dan full-scale = juga floating. Setelah itu klik BOOT lalu langsung bicara; kalau berhasil akan muncul baris `>> <teks hasil transkrip>`.
