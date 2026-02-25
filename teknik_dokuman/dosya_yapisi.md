## 3) Repo Dosya Yapısı (Yol Haritası)

> Bu bölüm “her dosya tek tek ne iş yapıyor?” sorusunu hızla cevaplamak için kategori bazlı bir rehberdir.

### 3.1 Kök (Root)
- CMakeLists.txt: raspi/PC seçmeli wrapper
- CMakePresets.json: preset’ler
- arduino.cpp: eski/deneysel kod olabilir (ana pipeline değil)
- VERSION: sürüm metni

### 3.2 Raspi — Yeni Savaş Pipeline’ı (raspi/)

**Konfigürasyon**
- raspi/config/sancak_config.yml: varsayılan parametreler (OpenCV FileStorage YAML)
- raspi/include/sancak/config_manager.hpp
- raspi/src/sancak/config_manager.cpp

**Ana orkestrasyon ve giriş**
- raspi/include/sancak/combat_pipeline.hpp
- raspi/src/sancak/combat_pipeline.cpp
- raspi/src/sancak/main.cpp: CLI argümanları ve çalıştırma

**Görü/Algoritma modülleri**
- YOLO:
  - raspi/include/sancak/yolo_detector.hpp
  - raspi/src/sancak/yolo_detector.cpp
- Balon segmentasyonu:
  - raspi/include/sancak/balloon_segmentor.hpp
  - raspi/src/sancak/balloon_segmentor.cpp
- Takip:
  - raspi/include/sancak/target_tracker.hpp
  - raspi/src/sancak/target_tracker.cpp
- Mesafe:
  - raspi/include/sancak/distance_estimator.hpp
  - raspi/src/sancak/distance_estimator.cpp
- Balistik:
  - raspi/include/sancak/ballistics_manager.hpp
  - raspi/src/sancak/ballistics_manager.cpp
- Nihai nişan:
  - raspi/include/sancak/aim_solver.hpp
  - raspi/src/sancak/aim_solver.cpp

**Kural motoru / State machine**
- raspi/include/core/types.hpp
- raspi/include/core/combat_state_machine.hpp (inline implementasyon)

**Tetik disiplini (güvenlik)**
- raspi/include/sancak/trigger_controller.hpp
- raspi/src/sancak/trigger_controller.cpp

**Donanım sürüş**
- raspi/include/turret_controller.hpp
- raspi/src/turret_controller.cpp (Linux termios; Windows’ta no-op)
- raspi/include/sancak/serial_comm.hpp + raspi/src/sancak/serial_comm.cpp (eski/alternatif seri katman)

**Network**
- Protokol:
  - raspi/include/network/protocol.hpp
- UDP Video:
  - raspi/include/network/udp_video_streamer.hpp
  - raspi/src/network/udp_video_streamer.cpp
- TCP Telemetry Server:
  - raspi/include/network/tcp_telemetry_server.hpp
  - raspi/src/network/tcp_telemetry_server.cpp
- Telemetry Sender (opsiyonel outbound push):
  - raspi/include/network/telemetry_sender.hpp
  - raspi/src/network/telemetry_sender.cpp

**Loglama**
- raspi/include/sancak/logger.hpp: basit placeholder {} formatlı logger

**Testler**
- raspi/tests/*.cpp: Catch2 unit testler

### 3.3 PC — GCS/Arayüz (PC/)

**Ana uygulama**
- PC/src/main.cpp: GLFW+OpenGL+ImGui init ve döngü
- PC/include/App.hpp
- PC/src/App.cpp: UI panelleri, video texture güncelleme, telemetri overlay

**Video alıcı (UDP)**
- PC/include/VideoReceiver.hpp
- PC/src/VideoReceiver.cpp: UDP listen + fragment reassembly + stb_image decode

**Telemetri istemcisi (TCP)**
- PC/include/TelemetryClient.hpp
- PC/src/TelemetryClient.cpp: TCP connect/retry + binary AimResult parse + line telemetry parse

**Arduino (opsiyonel)**
- PC/include/ArduinoController.hpp
- PC/src/ArduinoController.cpp

**Texture helper**
- PC/include/TextureHelper.hpp
- PC/src/TextureHelper.cpp

> Not: PC/README.md içinde Qt iddiaları varsa, gerçek build zinciri PC/CMakeLists.txt ile doğrulanmalıdır (ImGui/GLFW/OpenGL).

---
