### Saha Kalibrasyonu ve Checklist (Ek)

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
- ROI içinde ışık koşullarında kararlılık için:
  - `iff.friend_blue` ve `iff.foe_red` HSV aralıklarını sahada örnek kareyle ayarla.
  - Maske gürültüsü varsa ROI’yi çok büyütmek yerine HSV aralığını daraltmayı dene.
- Beklenen davranış:
  - Track bazlı majority voting + hysteresis ile tek-frame dalgalanmalar bastırılır.

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
  - `ballistics.manual_offset_px` sıfırken nişan noktası “ham merkez”de yakın mı?
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
