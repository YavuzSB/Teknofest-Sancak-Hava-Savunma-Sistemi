## Konfigürasyon (sancak_config.yml)

Dosya: `raspi/config/sancak_config.yml`

Format:
- OpenCV `cv::FileStorage` uyumlu YAML.

Önemli alanlar:
- `networking`:
  - `target_pc_ip` → `gcs_host`
  - `video_port` → `video_udp_port`
  - `telemetry_port` → `telemetry_tcp_port`
  - `telemetry_push_port` → outbound push (0 ise kapalı)
  - `jpeg_quality`, `udp_mtu_bytes`
- `targets`: kural motoru için min/max range + priority
- `geofence`: pan/tilt limit
- `iff`: HSV aralıkları
- `camera`: çözünürlük + FOV
- `yolo`: model yol + threshold’lar + sınıf isimleri
- `balloon`: HSV turuncu aralıkları ve geometrik eşikler
- `distance`: balon çapı + focal
- `ballistics`: lookup table + manuel offset
- `tracking`: IoU/mesafe eşikleri
- `trigger`: lock/burst/cooldown
- `serial`: port/baud/enabled
- `headless`, `autonomous`, `log_level`

---

## Bilinen Borçlar / Dikkat Edilecek Noktalar

- `raspi/src/sancak/main.cpp` içinde `--video` modu “not edilmiş” ama tam uygulanmamış (CameraController’da openFile + özel loop gerekebilir).
- `PC/README.md` içerikleri `PC/CMakeLists.txt` ile uyumsuz olabilir; gerçek kaynak CMake’tir.
- Telemetri portları: config ve C++ default’ları arasında alias’lar var; `ConfigManager` hem `network` hem `networking` şemasını destekliyor.
