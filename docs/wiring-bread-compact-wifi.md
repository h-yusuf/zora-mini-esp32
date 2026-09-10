# Wiring — bread-compact-wifi (INMP441 + SSD1306 + MAX98357A)

Board config `bread-compact-wifi` dipakai untuk dua modul ESP32-S3. Pin dipilih
compile-time lewat Kconfig `BREAD_COMPACT_WIFI_VARIANT`, jadi **jangan** edit
`config.h` untuk ganti board — cukup pilih profil:

```bash
XZ_BOARD=supermini ./scripts/xz.sh flash monitor
XZ_BOARD=n16r8     ./scripts/xz.sh flash monitor
```

Sumber pin: [`main/boards/bread-compact-wifi/config.h`](../main/boards/bread-compact-wifi/config.h)

---

## Ringkasan pin

| Fungsi | Pin modul | SuperMini | DevKit N16R8 |
|---|---|---|---|
| **Mic** INMP441 | WS | GPIO4 | GPIO4 |
| | SCK | GPIO5 | GPIO5 |
| | SD | GPIO6 | GPIO6 |
| | L/R | GND | GND |
| | VDD | 3V3 | 3V3 |
| | GND | GND | GND |
| **Amp** MAX98357A | DIN | GPIO7 | GPIO7 |
| | BCLK | **GPIO10** | GPIO15 |
| | LRC (WS) | **GPIO11** | GPIO16 |
| | VIN | 5V | 5V |
| | GND | GND | GND |
| | SD (mode) | biarkan / 3V3 | biarkan / 3V3 |
| | GAIN | biarkan (9 dB) | biarkan (9 dB) |
| **OLED** SSD1306 128x64 | SDA | **GPIO8** | GPIO41 |
| | SCL | **GPIO9** | GPIO42 |
| | VCC | 3V3 | 3V3 |
| | GND | GND | GND |
| Tombol BOOT (bawaan) | — | GPIO0 | GPIO0 |
| LED bawaan | — | *tidak terjangkau* | GPIO48 |
| Tombol touch (opsional) | — | *tidak terjangkau* | GPIO47 |
| Volume + / − (opsional) | — | *tidak terjangkau* | GPIO40 / GPIO39 |
| Lamp / LED tes | — | **GPIO13** | GPIO18 |

Yang **berbeda antar board** dicetak tebal. Header SuperMini hanya
mengeluarkan **GPIO0-13**, jadi pin di luar rentang itu (15, 16, 18, 39, 40,
47, 48) tidak bisa dipakai: speaker clock digeser ke GPIO10/11, lamp ke GPIO13,
dan LED/touch/volume dimatikan (`GPIO_NUM_NC`).

Pin dalam rentang 0-13 yang masih bebas: **GPIO1, 2, 3, 12**.

---

## Mic — INMP441

Modul I2S digital 24-bit. Firmware membaca **slot LEFT**, jadi `L/R` **harus ke GND**
(kalau ke VDD, mic mengirim di slot kanan dan yang terbaca cuma nol).

```
INMP441        ESP32-S3
  VDD  ──────  3V3
  GND  ──────  GND
  L/R  ──────  GND        (pilih channel kiri)
  WS   ──────  GPIO4
  SCK  ──────  GPIO5
  SD   ──────  GPIO6
```

Catatan penting:

- **Pad modul harus disolder.** Modul bulat INMP441 memakai pad setengah lubang
  (castellated). Kalau pin header hanya ditekan tanpa solder, jalur `SD` ngambang
  dan datanya jadi sampah — gejalanya raw I2S terbaca `00000000` atau `ffffffff`
  konstan, dan `Mic level` lompat antara 0/1 dan 32767 tanpa nilai tengah.
- Level sinyal wajar: diam `peak` puluhan–ratusan, bicara `peak` beberapa ribu
  dengan nilai tengah bervariasi. Kalau `peak` selalu tepat 32767, sinyalnya clipping.
- Skala 24-bit → int16 memakai `>> 16` di
  [`no_audio_codec.cc`](../main/audio/codecs/no_audio_codec.cc) (upstream `>> 12`
  bikin saturasi permanen). Kalau suara terlalu pelan untuk STT, kecilkan
  shift-nya (15 atau 14) untuk menaikkan gain.

---

## Amplifier — MAX98357A

DAC + amplifier kelas D dalam satu modul, input **I2S digital langsung** (tidak
perlu DAC tambahan).

```
MAX98357A      ESP32-S3
  VIN  ──────  5V         (3V3 juga jalan, tapi output jauh lebih pelan)
  GND  ──────  GND
  DIN  ──────  GPIO7
  BCLK ──────  GPIO10     (DevKit: GPIO15)
  LRC  ──────  GPIO11     (DevKit: GPIO16)
  SD   ──────  (biarkan)  → lihat tabel mode di bawah
  GAIN ──────  (biarkan)  → 9 dB
  +/−  ──────  speaker 4-8 Ω
```

**SD_MODE (pilih channel & shutdown).** Firmware mengirim mono di slot **LEFT**:

| SD_MODE | Mode |
|---|---|
| < 0.16 V (ke GND) | shutdown, speaker mati |
| resistor ke GND | channel kanan → **tidak ada suara** dengan config ini |
| ~1 V (divider) | (L+R)/2 → tetap berbunyi |
| > 1.4 V (biarkan / ke 3V3) | **channel kiri → yang dipakai** |

Sebagian besar breakout punya pull-up 100 kΩ di `SD`, jadi dibiarkan mengambang
sudah = channel kiri. Kalau ternyata senyap, tarik `SD` ke 3V3.

**GAIN.** Mengambang = 9 dB (default aman). Ke GND = 15 dB, ke VIN = 3 dB.

Catatan daya: MAX98357A bisa menarik ~1 A pada puncak volume. Kalau board reboot
saat suara keras, beri supply 5 V terpisah (ground tetap disatukan) atau
turunkan volume.

---

## OLED — SSD1306 128x64 I2C

```
SSD1306        ESP32-S3
  GND  ──────  GND
  VCC  ──────  3V3
  SCL  ──────  GPIO9      (DevKit: GPIO42)
  SDA  ──────  GPIO8      (DevKit: GPIO41)
```

- Alamat I2C **0x3C** (silkscreen `0x78` = notasi 8-bit dari 0x3C). Kalau modulnya
  di-jumper ke `0x7A`, alamatnya 0x3D dan `dev_addr` di
  [`compact_wifi_board.cc`](../main/boards/bread-compact-wifi/compact_wifi_board.cc)
  perlu diubah.
- Tipe panel dipilih di profil board: `CONFIG_OLED_SSD1306_128X64`.
- Warna di-invert (`esp_lcd_panel_invert_color(panel_, true)`) supaya mata menyala
  terang dengan latar gelap.
- Urutan pin header modul biasanya `GND VCC SCL SDA` — perhatikan **SCL sebelum SDA**,
  kebalik dari kebiasaan.

---

## Urutan verifikasi

1. **Flash** sesuai board: `XZ_BOARD=supermini ./scripts/xz.sh flash monitor`
2. **OLED** — firmware memprobe bus dulu. Log `Display found at 0x3C` lalu
   `Adding OLED display` berarti OK. Kalau muncul `No display on SDA=.. SCL=..`
   berarti tidak ada yang menjawab: cek 3V3/GND modul, urutan SDA/SCL, dan
   kontak solder. Device tetap boot normal tanpa display.
3. **Mic** — lihat `Mic level: peak=... avg=...`. Diam kecil, bicara naik ke
   ribuan dengan nilai bervariasi.
4. **Speaker** — device memainkan nada sukses setelah aktivasi. Kalau senyap:
   tarik `SD` ke 3V3, naikkan volume, pastikan `VIN` di 5 V.
5. **Percakapan** — klik BOOT sekali lalu **langsung** bicara (jangan ada jeda,
   dan jangan klik dua kali — klik kedua menutup sesi). Cari `>>` (hasil STT)
   dan `<<` (balasan) di log.

Log debug `Mic level` dan `raw i2s` bersifat sementara; hapus dari
`audio_service.cc` / `no_audio_codec.cc` kalau sudah tidak diperlukan.
