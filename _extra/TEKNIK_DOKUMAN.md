# Teknofest Sancak Hava Savunma Sistemi — Teknik Doküman (Tek Dosya)

Bu doküman, bu repodaki **mevcut CMake / C++ koduna** bakılarak hazırlanmış “tek dosyalık” teknik referanstır.

Amaç:
- Projeyi hiç bilmeyen birinin **ne var / nerede / nasıl çalışıyor** sorularını yanıtlamak
- Kullanılan **teknolojileri, dosya yapısını, protokolleri ve algoritmaları** gerekçeleriyle açıklamak
- Tasarım kararlarını “neden böyle?” ve “alternatifleri neydi?” ile birlikte özetlemek

> Not: Repo iki ana uygulamadan oluşur: **raspi/** (görü/otonom pipeline) ve **PC/** (GCS/arayüz). Kök dizindeki CMake sadece bunları seçmeli olarak derleyen bir wrapper’dır.

---

## 1) Hızlı Özet (1 Dakikada)

Sistem iki parçalıdır:

1. **Raspberry Pi (raspi/)**
   - Kameradan görüntü alır.
   - OpenCV DNN ile **ONNX YOLO26-Nano** modeli çalıştırır.
   - Tespit edilen hedef bbox’ı içinde **turuncu balon segmentasyonu** yapar.
   - Hedefi track eder (ID, hız) ve balistik/mesafe düzeltmeleriyle bir **nişan noktası** üretir.
   - Geofence + kural motoru + tetik disipliniyle **ateş izni** üretir.
   - (Opsiyonel) Video’yu **UDP JPEG fragment** ile PC’ye yollar.
   - (Opsiyonel) Telemetriyi **TCP** ile PC’ye yollar.
   - (Opsiyonel) Taret/Arduino’ya seri port üzerinden komut gönderir.

2. **PC GCS (PC/)**
   - UDP video stream’i alır, JPEG’i çözer ve ImGui panelinde gösterir.
   - TCP telemetriyi alır (binary “AimResult” frame), overlay (crosshair vb.) çizer.
   - Bazı komutları satır-bazlı TCP komutu olarak geri gönderebilir (WASD).

---

## 2) Derleme Sistemi ve Hedefler

### 2.1 Kök CMake (Wrapper)
- Dosya: `CMakeLists.txt`
- Amaç: CMake Presets vb. root’tan configure edilebilsin.
- Seçenekler:
  - `SANCAK_BUILD_RASPI` (Windows’ta varsayılan **OFF**, Linux’ta **ON**)
  - `SANCAK_BUILD_PC` (Windows’ta varsayılan **ON**, Linux’ta **OFF**)

Bu sayede Windows’ta OpenCV yokken bile `PC/` kısmı derlenebilir.

### 2.2 Raspi tarafı (raspi/CMakeLists.txt)
- C++ standard: C++17
- Bağımlılık: **OpenCV** (core/imgproc/highgui/videoio/dnn)
- Hedefler:
  - `sancak_network` (UDP video + TCP telemetri + TelemetrySender)
  - `sancak_config` (OpenCV FileStorage ile config)
  - `sancak_yolo`, `sancak_balloon`, `sancak_distance`, `sancak_ballistics`, `sancak_aim`, …
  - `sancak_combat` (ana executable)
  - Testler: Catch2 (FetchContent) ile `sancak_tests` vb.

### 2.3 PC tarafı (PC/CMakeLists.txt)
- UI: **Dear ImGui** (docking branch)
- Window/Input: **GLFW 3.4**
- Render: **OpenGL**
- JPEG decode: **stb_image**
- Network: Winsock2 (Windows)
- FetchContent ile bağımlılıklar otomatik çekilir, hedef tek `.exe`.

---

## 3) Repo Dosya Yapısı (Yol Haritası)

> Bu bölüm “her dosya tek tek ne iş yapıyor?” sorusunu hızla cevaplamak için **kategori bazlı** bir rehberdir.

### 3.1 Kök (Root)
- `CMakeLists.txt`: raspi/PC seçmeli wrapper
- `CMakePresets.json`: preset’ler
- `arduino.cpp`: eski/deneysel kod olabilir (ana pipeline değil)
- `VERSION`: sürüm metni

### 3.2 Raspi — Yeni Savaş Pipeline’ı (raspi/)

**Konfigürasyon**
- `raspi/config/sancak_config.yml`: varsayılan parametreler (OpenCV FileStorage YAML)
- `raspi/include/sancak/config_manager.hpp`
- `raspi/src/sancak/config_manager.cpp`

**Ana orkestrasyon ve giriş**
- `raspi/include/sancak/combat_pipeline.hpp`
- `raspi/src/sancak/combat_pipeline.cpp`
- `raspi/src/sancak/main.cpp`: CLI argümanları ve çalıştırma

**Görü/Algoritma modülleri**
- YOLO:
  - `raspi/include/sancak/yolo_detector.hpp`
  - `raspi/src/sancak/yolo_detector.cpp`
- Balon segmentasyonu:
  - `raspi/include/sancak/balloon_segmentor.hpp`
  - `raspi/src/sancak/balloon_segmentor.cpp`
- Takip:
  - `raspi/include/sancak/target_tracker.hpp`
  - `raspi/src/sancak/target_tracker.cpp`
- Mesafe:
  - `raspi/include/sancak/distance_estimator.hpp`
  - `raspi/src/sancak/distance_estimator.cpp`
- Balistik:
  - `raspi/include/sancak/ballistics_manager.hpp`
  - `raspi/src/sancak/ballistics_manager.cpp`
- Nihai nişan:
  - `raspi/include/sancak/aim_solver.hpp`
  - `raspi/src/sancak/aim_solver.cpp`

**Kural motoru / State machine**
- `raspi/include/core/types.hpp`
- `raspi/include/core/combat_state_machine.hpp` (inline implementasyon)

**Tetik disiplini (güvenlik)**
- `raspi/include/sancak/trigger_controller.hpp`
- `raspi/src/sancak/trigger_controller.cpp`

**Donanım sürüş**
- `raspi/include/turret_controller.hpp`
- `raspi/src/turret_controller.cpp` (Linux termios; Windows’ta no-op)
- `raspi/include/sancak/serial_comm.hpp` + `raspi/src/sancak/serial_comm.cpp` (eski/alternatif seri katman)

**Network**
- Protokol:
  - `raspi/include/network/protocol.hpp`
- UDP Video:
  - `raspi/include/network/udp_video_streamer.hpp`
  - `raspi/src/network/udp_video_streamer.cpp`
- TCP Telemetry Server:
  - `raspi/include/network/tcp_telemetry_server.hpp`
  - `raspi/src/network/tcp_telemetry_server.cpp`
- Telemetry Sender (opsiyonel outbound push):
  - `raspi/include/network/telemetry_sender.hpp`
  - `raspi/src/network/telemetry_sender.cpp`

**Loglama**
- `raspi/include/sancak/logger.hpp`: basit placeholder `{}` formatlı logger

**Testler**
- `raspi/tests/*.cpp`: Catch2 unit testler

### 3.3 PC — GCS/Arayüz (PC/)

**Ana uygulama**
- `PC/src/main.cpp`: GLFW+OpenGL+ImGui init ve döngü
- `PC/include/App.hpp`
- `PC/src/App.cpp`: UI panelleri, video texture güncelleme, telemetri overlay

**Video alıcı (UDP)**
- `PC/include/VideoReceiver.hpp`
- `PC/src/VideoReceiver.cpp`: UDP listen + fragment reassembly + stb_image decode

**Telemetri istemcisi (TCP)**
- `PC/include/TelemetryClient.hpp`
- `PC/src/TelemetryClient.cpp`: TCP connect/retry + binary AimResult parse + line telemetry parse

**Arduino (opsiyonel)**
- `PC/include/ArduinoController.hpp`
- `PC/src/ArduinoController.cpp`

**Texture helper**
- `PC/include/TextureHelper.hpp`
- `PC/src/TextureHelper.cpp`

> Not: `PC/README.md` içinde Qt iddiaları varsa, gerçek build zinciri `PC/CMakeLists.txt` ile doğrulanmalıdır (ImGui/GLFW/OpenGL).

---

## 4) Çalışma Topolojisi (Gerçek Akış)

### 4.1 Temel topoloji
- Raspi:
  - Kamera → `CombatPipeline` → (opsiyonel) UDP video → PC
  - Kamera → `CombatPipeline` → (opsiyonel) TCP telemetri → PC
  - `TurretController` → seri port → mikrodenetleyici

- PC:
  - UDP port dinler → görüntü paneli
  - TCP port bağlanır → telemetri + overlay

### 4.2 Telemetri “server-only” ve “push” ayrımı
Raspi tarafında iki telemetri yaklaşımı var:
- `TcpTelemetryServer`: **PC bağlanır** ve Raspi **server** olarak frame’leri gönderir.
- `TelemetrySender`: Raspi **client** olarak PC’ye **push** atar (config ile kapatılabilir).

Kodda port çakışması riskini azaltmak için `telemetry_push_port` ayrı bir port olarak tasarlanmıştır.

---

## 5) Network Protokolleri (SNK1/SNK2)

> Bu protokoller `raspi/include/network/protocol.hpp` içinde tanımlanır ve PC tarafında `PC/src/NetworkProtocol.hpp` üzerinden aynı magic/struct mantığıyla parse edilir.

### 5.1 UDP Video: SNK1 (Fragment’lı JPEG)

Amaç: UDP’nin “paket kaybı olabilir” doğasına uyacak şekilde, büyük JPEG’i MTU’ya bölüp yollamak.

- Magic: `SNK1`
- Header: `UdpJpegFragmentHeader` (packed, 36 byte)
- Her frame için:
  1. BGR frame → `cv::imencode(".jpg")`
  2. JPEG byte dizisi MTU’ya göre chunk’lara bölünür.
  3. Her chunk bir UDP datagram olarak gönderilir.

Önemli alanlar:
- `frame_id`: reassembly anahtarı
- `timestamp_us`: Raspi tarafında `system_clock` epoch microseconds (PC’de latency hesabı için)
- `chunk_index/chunk_count`
- `jpeg_bytes`, `chunk_offset`, `chunk_bytes`

PC tarafında reassembly:
- `frame_id` için `jpeg` buffer allocate edilir.
- Her chunk geldikçe offset’ine kopyalanır, “got” bitmap’i işaretlenir.
- Tüm chunk’lar gelince JPEG complete olur ve decode edilir.
- 500ms’den eski incomplete frame’ler temizlenir (GC).

Neden UDP + fragment?
- Düşük gecikme (TCP head-of-line blocking yok)
- Kayıp paket olduğunda “eski frame’leri beklemek” yerine yeni frame’e geçmek mümkün

Alternatifler:
- RTP/RTSP (daha standart ama daha ağır altyapı)
- WebRTC (NAT/ICE vb. karmaşıklık)
- TCP üzerinden MJPEG (kolay ama gecikme/kuyruk büyümesi riski)

### 5.2 TCP Telemetri: SNK2 (Binary frame)

Magic: `SNK2`
Header: `TcpMsgHeader` (8 byte)
Mesaj tipi:
- `kAimResultV1` (type=1)

Raspi `TcpTelemetryServer`’ın gönderdiği AimResultV1 payload (28 byte):
- `u32 frame_id`
- `f32 raw_x, raw_y`
- `f32 corrected_x, corrected_y`
- `i32 class_id`
- `u8 valid`
- `u8 reserved[3]`

PC `TelemetryClient` parse eder:
- Magic resync: magic mismatch → buffer’dan 1 byte sil, tekrar dene
- Version mismatch → aynı resync

Neden TCP?
- Telemetri küçük boyutlu; “tam ve sıralı” gelmesi tercih edilebilir.
- Basit: tek bağlantı, tek stream.

Alternatifler:
- UDP telemetri (paket kaybına toleranslı tasarım gerekir)
- Flatbuffers/Protobuf (şema evrimi için iyi; ek bağımlılık)

---

## 6) Raspi: CombatPipeline (Ana Orkestrasyon)

Ana sınıf: `sancak::CombatPipeline`

### 6.1 Başlatma (initialize)
Sıra:
1. Logger seviyesi (`config.log_level`)
2. YOLO modeli yükle (`YoloDetector::initialize`)
3. Segmentor init
4. AimSolver init (ballistics + distance)
5. Tracker init
6. TriggerController init (tolerans + lock/burst/cooldown)
7. CombatStateMachine init (target rules + geofence)
8. Kamera open
9. (Opsiyonel) `TurretController` oluştur (serial enabled)
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
11. TurretController’a `sendCommand(pan, tilt, fire)`
12. Network yayınları: UDP video + TCP aim frame + (opsiyonel) TelemetrySender

### 6.3 Watchdog / Fail-safe
- `last_valid_frame_time_` güncellenir.
- Eğer son “geçerli frame” üzerinden `> 500ms` geçtiyse:
  - `trigger_controller_.reset()`
  - `tracker_.reset()`
  - state `SafeLock`
  - taret için `sendSafeLock()` ve `fire=false`

Amaç: kamera kopuk/donuksa sistemin **ateş ve hareketi güvenli şekilde kesmesi**.

---

## 7) Algoritmalar (Detaylı)

### 7.1 YOLO26-Nano (OpenCV DNN + ONNX)
Dosyalar:
- `raspi/include/sancak/yolo_detector.hpp`
- `raspi/src/sancak/yolo_detector.cpp`

Akış:
1. **Letterbox**: Oran korunur, kalan kısım 114 gri ile pad edilir.
2. `blobFromImage`:
   - scale = `1/255`
   - BGR→RGB (`swapRB=true`)
   - CHW
3. `net.forward` (OpenCV DNN)
4. Çıkış parse:
   - beklenen format: `[1, (4+num_classes), N]`
   - transpose sonrası `[N, 4+nc]`
   - her satır için max class skorunu bul
   - `conf_threshold` altında kalanları ele
5. **NMSBoxes** ile kutuları filtrele
6. Letterbox koordinatlarını orijinal frame’e geri map et

Neden OpenCV DNN?
- Tek bağımlılık (OpenCV) ile inference + görüntü işlemenin aynı yerde olması
- Pi tarafında deployment kolaylığı

Alternatif: ONNX Runtime (ORT)
- Daha iyi performans/optimizasyon potansiyeli olabilir.
- Ancak ek bağımlılık/packaging maliyeti var.
- Kodda ORT’ye geçiş için yorum satırıyla “gelecek planı” not edilmiş.

### 7.2 IFF (Dost/Düşman) — HSV ile renk sayımı
Uygulama yeri: `CombatPipeline::processFrame` içinde YOLO detections üzerinde.

Yöntem:
- Her bbox için ROI kes
- BGR→HSV
- Kırmızı maske (`iff.foe_red`) ve mavi maske (`iff.friend_blue`)
- `countNonZero` ile kırmızı/mavi piksel say
- red>blue → Foe, blue>red → Friend, aksi Unknown

Neden basit HSV?
- Sahada hızlı kalibrasyon
- Çok düşük CPU maliyeti

Alternatifler:
- Modelin içine IFF sınıfını entegre etmek (daha stabil ama dataset ihtiyacı)
- Dairesel/ok işareti tespiti (shape-based)

### 7.3 IFF Stabilizasyonu — Majority voting + hysteresis
Dosya: `raspi/src/sancak/target_tracker.cpp`

Problem:
- Tek frame’de renk maskeleme dalgalanabilir (ışık, motion blur, ROI sınırı).

Çözüm:
- Her track için `affiliation_history` deque (son 5 frame)
- “mode/majority” seçilir
- Eşitlik durumunda **önceki değer korunur** (hysteresis)

### 7.4 Multi-target Tracking — IoU + merkez mesafesi fallback
Dosya: `raspi/src/sancak/target_tracker.cpp`

1) Greedy IoU matching:
- IoU matrisi → threshold üstü adaylar
- IoU’ya göre büyükten küçüğe sırala
- Greedy: track/det yalnız 1 kez eşleşir

2) Fallback matching (IoU kaçınca):
- IoU eşleşmeyen track/det çiftlerinde merkez mesafesi
- `max_center_distance_px` içinde olanlar
- En küçük mesafeden başlayarak greedy eşleştir

3) Velocity:
- `raw_vel = new_center - prev_center`
- EMA: `v = alpha*v + (1-alpha)*raw_vel`

4) Track yaşam döngüsü:
- eşleşmeyen track: `lost_frames++`
- `lost_frames > max_lost_frames` → sil

Neden SORT/DeepSORT değil?
- Bu repo “hafif / bağımlılıksız” yaklaşımıyla ilerlemiş.
- Kamera/target sayısı düşük varsayımıyla IoU+greedy yeterli.

Alternatifler:
- Kalman filter + Hungarian matching (klasik SORT)
- ReID (DeepSORT) — daha ağır

### 7.5 Balon segmentasyonu (bbox içinde)
Dosyalar:
- `raspi/include/sancak/balloon_segmentor.hpp`
- `raspi/src/sancak/balloon_segmentor.cpp`

Algoritma:
1. bbox ROI kırp
2. HSV’ye çevir
3. Turuncu maske
4. Morfoloji (kernel + iter)
5. Kontur bul
6. En büyük konturu seç
7. `minEnclosingCircle` ile merkez + radius
8. Dairesellik kontrolü (min_circularity)
9. ROI koordinatını global frame’e taşı

Neden segmentasyon?
- YOLO bbox merkezi “balon merkezi” ile aynı olmayabilir.
- Nişan için daha stabil “gerçek merkez” gerekir.

Alternatifler:
- Modeli doğrudan keypoint/center predict edecek şekilde eğitmek
- Instance segmentation (daha ağır)

### 7.6 Mesafe tahmini (pinhole)
Dosyalar:
- `raspi/include/sancak/distance_estimator.hpp`
- `raspi/src/sancak/distance_estimator.cpp`

Formül:
- Balon çapından: $D = (S \cdot f) / s$
  - $S$: gerçek çap (m)
  - $f$: odak (px)
  - $s$: görüntüde piksel çap (px)

Bu projede iki tahmin birleştirilebilir:
- Balon yarıçapından
- bbox yüksekliğinden (yaklaşık gerçek yükseklik varsayımıyla)

`combined()`:
- Eğer ikisinin confidence’ı varsa ağırlıklı ortalama

Alternatifler:
- Stereo / depth camera
- Monocular depth estimation modeli

### 7.7 Balistik düzeltme (lookup table ağırlıklı)
Dosyalar:
- `raspi/include/sancak/ballistics_manager.hpp`
- `raspi/src/sancak/ballistics_manager.cpp`

Bileşenler:
1. Paralaks:
   - Kamera-namlu offset’i üzerinden açı farkı (zeroing mesafesine göre)
2. Drop:
   - basit yerçekimi modeli: $drop = 0.5 g t^2$, $t = D/v$
3. Lead:
   - hedef hızı × (uçuş + işlem gecikmesi)
4. Lookup table:
   - sahada ölçülen (distance → dx/dy) düzeltmeleri
   - doğrusal interpolasyon
5. Manuel offset:
   - sahada ince ayar

Neden lookup table?
- Balistik model her zaman platform/mermi/servo geometrisini tam yakalayamaz.
- Saha verisiyle kalibrasyon pratikte daha güvenilir.

Alternatifler:
- Tam fizik model + rüzgar + drag
- Online kalibrasyon (feedback sensörleri)

### 7.8 AimSolver (tüm düzeltmelerin birleşimi)
Dosyalar:
- `raspi/include/sancak/aim_solver.hpp`
- `raspi/src/sancak/aim_solver.cpp`

Adımlar:
1. raw_center = balon merkezi
2. distance = DistanceEstimator.combined(...)
3. correction = BallisticsManager.calculate(distance, velocity, fps)
4. corrected = raw + (dx, dy)
5. corrected clamp (frame sınırları)

### 7.9 Geofence
- `config.geofence`: pan/tilt minimum–maksimum
- İki yerde uygulanır:
  1) `core::CombatStateMachine`: mevcut pan/tilt sınır dışıysa `SafeLock`
  2) `CombatPipeline`: desired pan/tilt sınır dışıysa SafeLock + fire=false

Neden iki katman?
- “Kural motoru” ve “kontrol döngüsü” birbirinden bağımsız güvenlik katmanları.

### 7.10 TriggerController (FPS bağımsız tetik disiplini)
Dosyalar:
- `raspi/include/sancak/trigger_controller.hpp`
- `raspi/src/sancak/trigger_controller.cpp`

State machine:
- SEARCHING → LOCKING → FIRING → COOLDOWN

Kilitlenme:
- Target center ile crosshair center arası mesafe `aim_tolerance_px` içinde **lock_duration_ms** kadar kalmalı.

Ateş:
- FIRING’de `burst_duration_ms` boyunca fire=true
- Sonra `cooldown_ms` boyunca fire=false

Neden zaman bazlı?
- FPS değişken olduğunda frame-count bazlı kilit süreleri sahada tutarsız olur.

---

## 8) Kural Motoru (CombatStateMachine)

Dosya: `raspi/include/core/combat_state_machine.hpp`

Girdi:
- `std::vector<core::TrackedTarget>`
- `current_pan/current_tilt`

Karar:
- Geofence dışı → SafeLock
- Target rules (YAML) ile target class’a göre:
  - min/max range
  - priority (küçük sayı = daha yüksek)
- Friend ise elenir
- En iyi hedef seçilir
- **Lost target failsafe**: `is_lost==true` ise Engaging’e geçmez
- Eğer `affiliation==Foe` ise Engaging

Neden bu kadar basit?
- Bu katman “politikayı” belirler: hangi hedefe, hangi sırayla, hangi menzilde.
- Karmaşık ML yerine açık, test edilebilir kurallar.

Alternatifler:
- Utility-based scoring (mesafe, confidence, hız vb. ağırlıklarla)
- RL/learned policy (yüksek risk/karmaşıklık)

---

## 9) Donanım Kontrolü: TurretController

Dosyalar:
- `raspi/include/turret_controller.hpp`
- `raspi/src/turret_controller.cpp`

Özellikler:
- Linux’ta termios ile seri port açar (`/dev/ttyUSB0`)
- Ayrı worker thread:
  - komut kuyruğunu tüketir
  - safe lock komutunu önceliklendirir
- Komut protokolü (string):
  - Hareket: `<M:pan,tilt>\n` (float, 2 ondalık)
  - Ateş: `<F:1>\n` veya `<F:0>\n`
  - SafeLock: `<S>\n`

Tasarım notu:
- `sendCommand` sadece değişiklik olduğunda kuyruğa ekler (spam azaltma).
- Port koparsa worker yeniden açmayı dener.

Alternatifler:
- Binary protokol (CRC, framing)
- CAN bus
- ROS2 topic/service

---

## 10) PC GCS: UI + Ağ Thread’leri

### 10.1 UI (ImGui)
- `PC/src/App.cpp` “immediate-mode” yaklaşımıyla panelleri çizer.
- Video texture her frame güncellenir.
- Telemetri verisi geldiğinde overlay (crosshair vb.) çizilir.

Neden ImGui?
- Çok hızlı prototipleme
- Tek exe, minimum bağımlılık

Alternatifler:
- Qt (daha büyük ekosistem, daha ağır dağıtım)
- Web tabanlı UI (Electron, browser)

### 10.2 VideoReceiver (UDP)
- Thread açar ve UDP portu dinler.
- SNK1 fragment varsa reassembly yapar.
- JPEG complete olunca `stb_image` ile decode eder.
- En son frame’i thread-safe şekilde UI’ya sunar.

### 10.3 TelemetryClient (TCP)
- Bağlanır, koparsa retry/backoff yapar.
- Gelen stream’de önce binary AimResult frame parse etmeye çalışır.
- Eğer binary değilse newline-delimited key=value telemetry parse eder.
- Ayrıca UI’dan gelen komutları newline ile server’a yollar.

---

## 11) Konfigürasyon (sancak_config.yml)

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

## 12) Bilinen Borçlar / Dikkat Edilecek Noktalar

- `raspi/src/sancak/main.cpp` içinde `--video` modu “not edilmiş” ama tam uygulanmamış (CameraController’da openFile + özel loop gerekebilir).
- `PC/README.md` içerikleri `PC/CMakeLists.txt` ile uyumsuz olabilir; gerçek kaynak CMake’tir.
- Telemetri portları: config ve C++ default’ları arasında alias’lar var; `ConfigManager` hem `network` hem `networking` şemasını destekliyor.

### 12.1) Saha Kalibrasyonu ve Checklist (Ek)

Bu bölüm “sistemi ayağa kaldırdım ama sahada nasıl doğrularım/ayarlarım?” için pratik bir sıradır. Parametrelerin ana kaynağı `raspi/config/sancak_config.yml`’dir.

Başlamadan (kurulum)
- PC ve Raspi aynı ağda mı? (IP/Subnet)
- Raspi’de kamera görüntüsü alınıyor mu?
- Seri port doğru mu? (`serial.enabled`, `serial.port`, `serial.baud`)
- Portlar çakışmıyor mu?
  - UDP video: `video_udp_port`
  - TCP telemetri server: `telemetry_tcp_port`
  - Opsiyonel outbound push: `telemetry_push_port` (0 ise kapalı)

Görü ve model doğrulama
- Model dosyası ve giriş boyutu:
  - `yolo.model_path` mevcut mu?
  - `yolo.input_width/height` modelin beklediği ile uyumlu mu?
- İlk hedef tespiti:
  - `yolo.conf_threshold` / `yolo.nms_threshold` ile “çok kaçırma” vs “çok false positive” dengesini kur.

IFF (dost/düşman) kalibrasyonu (HSV)
- ROI içinde ışık koşulları değişiminde kararlılık için:
  - `iff.friend_blue` ve `iff.foe_red` HSV aralıklarını sahada örnek kareyle ayarla.
  - Maske gürültüsü varsa ROI’yi çok büyütmek yerine HSV aralığını daraltmayı dene.
- Beklenen davranış:
  - Track bazlı majority voting + hysteresis ile tek-frame dalgalanmaları bastırılır.

Balon segmentasyonu kalibrasyonu (HSV + morfoloji)
- `balloon.hsv_orange` aralığını “güneş/kapalı hava” için ayrı ayrı ölç.
- `balloon.min_radius_px` ve `balloon.min_circularity`:
  - Çok sık eliyorsa eşiği düşür.
  - Rastgele lekeleri balon sanıyorsa eşiği yükselt.

Mesafe (pinhole) kalibrasyonu
- Hedef çapı:
  - `distance.balloon_diameter_m` sahadaki gerçek balonla aynı olmalı.
- Odak (px):
  - `distance.focal_length_px` için pratik yöntem: bilinen mesafede (örn. 10 m) balonu görüntüle, balon piksel çapını ölç, $f = (D \cdot s)/S$ ile geri hesapla.

Balistik/LUT kalibrasyonu (en pratik adım)
- Önce “sıfır” davranışı doğrula:
  - `ballistics.manual_offset_px` sıfırken nişan noktası “ham merkez”e yakın mı?
- Sonra LUT:
  1. 3–5 mesafe noktası seç (örn. 10/15/20/25/30 m).
  2. Her noktada stabil bir hedefle, ham merkezden sapmayı ölç (dx/dy).
  3. Bu sapmaları lookup-table’a gir.
  4. Arada kalan mesafelerde doğrusal interpolasyonun çalıştığını test et.

Takip (tracker) parametreleri
- “ID zıplıyor” ise:
  - `tracking.iou_threshold` düşürmeyi veya `tracking.max_center_distance_px` artırmayı dene.
- “Eski hedefe yapışıyor” ise:
  - `tracking.max_lost_frames` düşürmeyi dene.

Tetik disiplini (güvenlik) doğrulaması
- `trigger.aim_tolerance_px`: çok küçükse hiç kilitlenmez, çok büyükse yanlış kilitlenir.
- `trigger.lock_duration_ms`: hedef merkezde kısa süre durduğunda ateş etmemeli.
- `trigger.burst_duration_ms` ve `trigger.cooldown_ms`: sahadaki emniyet gereksinimine göre kısa tutulmalı.

Geofence güvenlik testi
- `geofence.pan_min/max` ve `geofence.tilt_min/max` sınırlarında şu beklenir:
  - Sınır dışına istek gelince SafeLock + fire=false.
- Test: hedefi ekranın köşelerine taşıyarak sınır davranışını gözle.

Fail-safe senaryoları (mutlaka)
- Kamera kesilmesi (watchdog):
  - Kamerayı geçici kapat/çek → ~500ms içinde SafeLock + fire=false beklenir.
- Lost-target: hedef kadrajdan çıkınca Engaging devam etmemeli.
- Telemetri/video kesilmesi:
  - PC tarafı bağlantıyı kaybetse bile Raspi’nin safety katmanları çalışmalı (ateş sadece yerel kurallara bağlı olmalı).

### 12.2) Operasyonel Test Senaryoları (Adım Adım)

Bu bölüm, sahada “çalışıyor mu?” sorusunu hızlı yanıtlamak için **gözlemlenebilir çıktılara** dayanır:
- PC tarafı: status bar metinleri + canlı log konsolu + video paneli overlay.
- Raspi tarafı: stderr’e basılan `SANCAK_LOG_*` logları.

#### 12.2.1) PC uygulamasını doğru hedefe bağlama (konfigürasyon testi)

1) PC uygulamasını başlatmadan önce (opsiyonel) ortam değişkenlerini ayarla:
   - `RASPI_HOST` (ör. `192.168.1.50`)
   - `VIDEO_UDP_PORT` (varsayılanla aynı olmalı)
   - `TEL_TCP_PORT` (varsayılanla aynı olmalı)

Örnek (PowerShell, sadece o terminal oturumu için)
```powershell
$env:RASPI_HOST="192.168.1.50"
$env:VIDEO_UDP_PORT="5001"
$env:TEL_TCP_PORT="5002"
```

Örnek (CMD, sadece o oturum için)
```bat
set RASPI_HOST=192.168.1.50
set VIDEO_UDP_PORT=5001
set TEL_TCP_PORT=5002
```

Beklenen çıktı (PC status bar)
- Uygulama açılır açılmaz:
  - `Video: Dinleniyor`
  - `Telemetri: Baglaniyor...` (hemen ardından bağlanırsa `Bagli`)

Beklenen çıktı (PC log konsolu)
- En azından şu satırları görmen beklenir:
  - `Sunucuya baglaniliyor: <RASPI_HOST>:<TEL_TCP_PORT>`
  - `TCP baglandi`
  - Durum değişimi logları: `Telemetri baglaniyor...` → `Telemetri baglandi`

Sorun/teşhis
- `TCP baglanti denemesi basarisiz` görüyorsan: IP/port, firewall veya Raspi tarafında server’ın dinlememesi.

#### 12.2.2) Telemetri protokolü (SNK2) “sağlamlık” testi

Amaç: TCP stream bozulsa bile PC’nin resync olup devam edebilmesi.

Beklenen çıktı (PC log konsolu)
- Zaman zaman (özellikle hatalı veri gelirse) şu uyarılar görülebilir:
  - `Bozuk telemetri paketi alindi (Magic mismatch)!`
  - `Bozuk telemetri paketi alindi (Version mismatch)!`
  - `Bozuk telemetri paketi alindi (Frame type/size)!`

Yorum
- Bu uyarılar **her frame’de** sürekli akıyorsa sistem yanlış porta bağlanıyor olabilir veya karşı uç SNK2 frame göndermiyordur.

#### 12.2.3) AimResult overlay testi (hedef varken/yokken)

Amaç: Raspi’nin ürettiği nişan noktasının PC video panelinde doğru şekilde çizildiğini doğrulamak.

1) Hedef yokken
- Beklenen davranış:
  - Video panelinde ya overlay hiç çizilmez ya da “son bilinen noktaya” kırmızımsı crosshair görülebilir.
  - `aim.valid` sahte true olmamalı.

2) Hedef varken (balon tespit + segment + aim)
- Beklenen davranış:
  - Video panelinde **yeşil crosshair** görünür.
  - Crosshair yakınında etiket: `ID:<class_id>  (x,y)`

Beklenen telemetri alanları (PC tarafı parse edilen AimResult)
- `AimResult.valid == true`
- `AimResult.corrected_x/y` görüntü boyutları içinde olmalı:
  - $0 \le x < width$
  - $0 \le y < height$
- `AimResult.frame_id` zamanla artar (her aim frame’de).

Sorun/teşhis
- Crosshair panelin yanlış yerinde ise: kamera çözünürlüğü/telemetrideki koordinatların aynı referansta olduğundan emin ol (Raspi frame boyutu ile PC’nin aldığı video boyutu eşleşmeli).

#### 12.2.4) UDP video akışı testi (SNK1)

Amaç: UDP fragment’lı JPEG akışının PC tarafında toparlanıp stabil görüntü verdiğini doğrulamak.

Beklenen çıktı (PC status bar)
- Raspi video gönderiyorsa: `Video: Aliniyor`
- Gönderim yoksa: `Video: Dinleniyor` veya `Bekleniyor`

Beklenen davranış
- Görüntü “takıla takıla” gelse bile zamanla akıcı bir şekilde yenilenmeli.
- Çok paket kaybında bazı frame’ler atlanabilir; bu normaldir (TCP gibi bekleyip gecikmeyi büyütmez).

Sorun/teşhis
- `Video: Hata!` → PC’nin UDP bind IP/port’u, izinler veya port çakışması.

#### 12.2.5) Güvenlik senaryoları (fail-safe) — gözlemlenebilir çıktılar

1) Watchdog (kamera kesilmesi)
- Adım: Raspi’de kamerayı geçici olarak devre dışı bırak (çek/kapat).
- Beklenen:
  - Raspi kontrol döngüsü SafeLock’a gider ve fire false olur.
  - PC tarafında kısa süre sonra yeşil overlay kaybolmalı (veya `valid=false` nedeniyle kırmızı “son nokta” davranışına düşmeli).

2) Lost-target (hedef kadrajdan çıkması)
- Adım: Balonu kadrajdan çıkar.
- Beklenen:
  - Engaging devam etmemeli; ateş komutu kesilmeli.
  - PC’de `AimResult.valid` true kalıyorsa bile crosshair “hedef dışı”na sürüklenmemeli (pratikte valid=false’a düşmesi tercih edilir).

3) Geofence
- Adım: Hedefi görüntünün uçlarına taşı (servo limitlerine zorlayacak şekilde).
- Beklenen:
  - Geofence dışı nişan isteklerinde SafeLock + fire=false.
  - PC tarafında overlay güvenli şekilde kaybolmalı veya valid=false’a düşmeli.

#### 12.2.6) Örnek Raspi log formatı (hızlı tanı)

Raspi logger formatı:
- `HH:MM:SS.mmm [INF] mesaj...`
- Debug ve daha düşük seviyede satır sonunda: `(dosya.cpp:123)`

Pratik kullanım
- Sahada “ne oldu?” için önce `WRN/ERR` satırlarına bak, sonra `INF` akışında pipeline’ın ilerleyip ilerlemediğini doğrula.

> Not: PC arayüzündeki bazı butonlar (`<DETECT:START>`, `<MODE:FULL_AUTO>`, `<GEOFENCE:...>`) TCP üzerinden komut satırı gönderir; mevcut Raspi `TcpTelemetryServer` bu satırları yalnızca tüketir, uygulama davranışını değiştiren bir komut parser’ı bu repo halinde görünmüyor. Bu yüzden bu butonlara basmak UI’da durum mesajı üretebilir ama Raspi modunu gerçekten değiştirmeyebilir.

---

## 13) Hızlı Komutlar

### 13.1 PC build (Windows)
```bash
cmake -S PC -B PC/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build PC/build --config Release
```

### 13.2 Raspi build (Linux/Raspberry Pi)
```bash
cmake -S raspi -B raspi/build -DCMAKE_BUILD_TYPE=Release
cmake --build raspi/build -j
ctest --test-dir raspi/build
```

---

## 14) Akış Diyagramı (Mermaid)

```mermaid
flowchart LR
  CAM[CameraController] --> YOLO[YoloDetector (OpenCV DNN)]
  YOLO --> IFF[IFF HSV ROI]
  IFF --> TRK[TargetTracker (IoU + EMA)]
  TRK --> SEG[BalloonSegmentor (HSV+Contour)]
  SEG --> DST[DistanceEstimator]
  DST --> BAL[BallisticsManager (LUT+offset)]
  BAL --> AIM[AimSolver]
  AIM --> SM[CombatStateMachine + Geofence]
  SM --> TRG[TriggerController]
  TRG --> TURRET[TurretController]
  AIM --> UDP[UdpVideoStreamer]
  AIM --> TCP[TcpTelemetryServer]
```
