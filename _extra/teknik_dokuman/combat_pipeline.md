## 6) Raspi: CombatPipeline (Ana Orkestrasyon)

Ana sınıf: sancak::CombatPipeline

### 6.1 Başlatma (initialize)
Sıra:
1. Logger seviyesi (config.log_level)
2. YOLO modeli yükle (YoloDetector::initialize)
3. Segmentor init
4. AimSolver init (ballistics + distance)
5. Tracker init
6. TriggerController init (tolerans + lock/burst/cooldown)
7. CombatStateMachine init (target rules + geofence)
8. Kamera open
9. (Opsiyonel) TurretController oluştur (serial enabled)
10. (Opsiyonel) UDP Video streamer start
11. (Opsiyonel) TCP Telemetry server start (+ opsiyonel TelemetrySender)

### 6.2 Frame işleme (processFrame)
Özet akış:
1. FPS ölçümü
2. Watchdog (kamera donma/kopma)
3. YOLO detect
4. IFF renk analizi (ROI içinde HSV threshold)
5. Tracker update
6. Her track için:
   - Segmentor ile balon bul
   - AimSolver ile nişan hesapla
7. State machine ile karar (Searching/Tracking/Engaging/SafeLock)
8. FOV mapping: piksel offset → pan/tilt derece
9. Geofence kontrol
10. TriggerController ile fire_flag üret (otonom + engaging)
11. TurretController’a sendCommand(pan, tilt, fire)
12. Network yayınları: UDP video + TCP aim frame + (opsiyonel) TelemetrySender

### 6.3 Watchdog / Fail-safe
- last_valid_frame_time_ güncellenir.
- Eğer son “geçerli frame” üzerinden > 500ms geçtiyse:
  - trigger_controller_.reset()
  - tracker_.reset()
  - state SafeLock
  - taret için sendSafeLock() ve fire=false

Amaç: kamera kopuk/donuksa sistemin ateş ve hareketi güvenli şekilde kesmesi.

---
