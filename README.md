# Zora Mini ESP32 — Chatbot AI Berbasis MCP

## Pengantar

Zora Mini adalah firmware chatbot AI untuk ESP32 yang jadi pintu masuk interaksi suara ke berbagai large language model (Qwen, DeepSeek, dll), dan bisa mengontrol banyak perangkat sekaligus lewat protokol MCP.

## Update Terbaru

- Mainline sekarang target ESP-IDF v6.0 ke atas, versi stabil yang disarankan v6.0.2.
- Kode kripto MQTT dan BluFi sudah pindah ke PSA Crypto. Pemisahan komponen IDF 6 dan kompatibilitas dependency pihak ketiga juga sudah dibereskan.
- Concurrency di audio pipeline, validasi paket MQTT/UDP, dan pemilihan release matrix diperkuat.
- ESP-IDF v5.5 cuma dipertahankan buat board lama yang memang masih perlu. ESP32-P4 Rev1 dan Rev3 sudah didukung penuh di IDF 6 dengan ESP-SR 2.4.7 — detail kompatibilitas & validasi board ada di [Panduan Migrasi ESP-IDF 6.0](docs/esp-idf-6-migration.md).

## Modul Board per Branch

Konfigurasi board/module dipecah per branch supaya tiap varian hardware gampang dilacak dan gak numpuk di satu branch. Untuk sekarang tersedia dua branch:

- `esp32/supermini` — konfigurasi untuk board ESP32 SuperMini
- `esp32/n16r8` — konfigurasi untuk board dengan varian flash/PSRAM N16R8

Checkout branch sesuai module yang dipakai sebelum build. Branch lain buat module tambahan bakal menyusul.

### Fitur yang Sudah Ada

- Konektivitas Wi-Fi, Ethernet kabel, USB RNDIS, serta 4G Cat.1 via ML307/EC801E atau NT26; board yang didukung bisa pindah antara Wi-Fi dan 4G
- Wake word offline pakai [ESP-SR](https://github.com/espressif/esp-sr), termasuk wake word custom
- Dua jalur komunikasi: [WebSocket](docs/websocket.md) dan [MQTT + UDP](docs/mqtt-udp.md)
- Streaming audio Opus dengan pipeline ASR + LLM + TTS konvensional, plus model voice realtime end-to-end; hardware yang support AEC bisa full-duplex realtime
- Pengenalan pembicara lewat [3D Speaker](https://github.com/modelscope/3D-Speaker)
- Layar OLED/LCD dengan emoji dan ekspresi, plus input visual dari kamera di board yang mendukung
- Tampilan status baterai dan manajemen daya
- 39 bahasa antarmuka, prompt suara lokal kalau tersedia, fallback ke Inggris
- Dukungan chip ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3, dan ESP32-P4
- Provisioning Wi-Fi lewat hotspot atau BluFi
- MCP sisi device buat kontrol hardware (speaker, LED, servo, GPIO, dll)
- MCP sisi cloud buat perluas kemampuan large model (kontrol smart home, operasi desktop PC, pencarian knowledge, email, dll)
- Wake word, font, emoji, dan background chat custom lewat editor web ([Custom Assets Generator](https://github.com/78/xiaozhi-assets-generator))

## Hardware

Firmware ini jalan di banyak board development ESP32-S3/C3/C6/P4 dari berbagai vendor (LiChuang, Espressif ESP32-S3-BOX, M5Stack, Waveshare, LILYGO, dan lain-lain), plus breadboard DIY. Daftar board dan konfigurasi pin lengkap ada di direktori `main/boards/`, dipilih lewat menuconfig sebelum build.

## Software

### Flashing Firmware

Buat pemula, disarankan pakai firmware yang sudah dikompilasi supaya gak perlu setup development environment dulu.

Secara default firmware terhubung ke server [xiaozhi.me](https://xiaozhi.me). User personal bisa daftar akun buat pakai model realtime Qwen gratis.

### Development Environment

- Cursor atau VSCode
- Install plugin ESP-IDF. Disarankan [ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2) atau rilis stabil v6.0 ke atas. ESP-IDF v5.5.2 cuma buat kompatibilitas board lama
- Linux lebih enak dipakai dibanding Windows — kompilasi lebih cepat, masalah driver lebih sedikit
- Project ini pakai Google C++ code style, ikuti gaya ini kalau mau kontribusi kode

### Dokumentasi Developer

- [Panduan Migrasi ESP-IDF 6.0](docs/esp-idf-6-migration.md) — kompatibilitas SDK, perubahan komponen, dukungan hardware lama, status validasi board
- [Panduan Custom Board](docs/custom-board.md) — cara bikin konfigurasi board sendiri
- [Penggunaan Protokol MCP untuk Kontrol IoT](docs/mcp-usage.md)
- [Alur Interaksi Protokol MCP](docs/mcp-protocol.md) — implementasi MCP sisi device
- [Dokumen Protokol Hybrid MQTT + UDP](docs/mqtt-udp.md)
- [Dokumen Detail Protokol WebSocket](docs/websocket.md)

## Konfigurasi Large Model

Kalau perangkat sudah terhubung ke server resmi, konfigurasi model bisa diatur lewat console [xiaozhi.me](https://xiaozhi.me).

## Project Open Source Terkait

Buat deploy server sendiri di komputer pribadi, cek project open source berikut:

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) — server Python
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) — server Java
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) — server Golang
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) — server Golang

Project client lain yang pakai protokol komunikasi yang sama:

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) — client Python
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) — client Android
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) — client Linux
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) — firmware chip Bluetooth
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) — firmware QuecPython

Tools asset custom:

- [78/xiaozhi-assets-generator](https://github.com/78/xiaozhi-assets-generator) — generator wake word, font, emoji, background custom

## Tentang Project

Ini project open-source ESP32, dirilis dengan lisensi MIT — bebas dipakai siapa saja, termasuk buat komersial.

Harapannya project ini bantu siapa pun yang mau belajar development AI hardware dan menerapkan large language model yang terus berkembang ke perangkat fisik nyata.

Ada ide atau saran, silakan buka Issue.

---

Project ini rewrite dari [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32.git).
