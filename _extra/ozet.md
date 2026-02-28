# Teknofest Sancak Hava Savunma Sistemi — Teknik Doküman

Bu doküman, repodaki mevcut CMake/C++ koduna bakılarak hazırlanmış teknik referanstır.

Amaç:
- Projeyi hiç bilmeyen birinin ne var/nerede/nasıl çalışıyor sorularını yanıtlamak
- Kullanılan teknolojileri, dosya yapısını, protokolleri ve algoritmaları gerekçeleriyle açıklamak
- Tasarım kararlarını neden böyle ve alternatifleriyle birlikte özetlemek

> Not: Repo iki ana uygulamadan oluşur: raspi/ (görü/otonom pipeline) ve PC/ (GCS/arayüz). Kök dizindeki CMake sadece bunları seçmeli olarak derleyen bir wrapper’dır.

---

## 1) Hızlı Özet

Sistem iki parçalıdır:

1. **Raspberry Pi (raspi/)**
   - Kameradan görüntü alır.
   - OpenCV DNN ile ONNX YOLO26-Nano modeli çalıştırır.
   - Tespit edilen hedef bbox’ı içinde turuncu balon segmentasyonu yapar.
   - Hedefi track eder (ID, hız) ve balistik/mesafe düzeltmeleriyle bir nişan noktası üretir.
   - Geofence + kural motoru + tetik disipliniyle ateş izni üretir.
   - (Opsiyonel) Video’yu UDP JPEG fragment ile PC’ye yollar.
   - (Opsiyonel) Telemetriyi TCP ile PC’ye yollar.
   - (Opsiyonel) Taret/Arduino’ya seri port üzerinden komut gönderir.

2. **PC GCS (PC/)**
   - UDP video stream’i alır, JPEG’i çözer ve ImGui panelinde gösterir.
   - TCP telemetriyi alır (binary “AimResult” frame), overlay (crosshair vb.) çizer.
   - Bazı komutları satır-bazlı TCP komutu olarak geri gönderebilir (WASD).

---

## 2) Doküman Haritası (Hangi Konu Nerede?)

Bu klasördeki dosyalar tek bir uzun dokümanın parçalara bölünmüş halidir. Aradığın konuyu hızlı bulmak için:

- [teknik_dokuman/ozet.md](teknik_dokuman/ozet.md): Bu sayfa — genel resim ve doküman indeksi
- [teknik_dokuman/build.md](teknik_dokuman/build.md): CMake build mantığı, PC/Raspi derleme-çalıştırma adımları
- [teknik_dokuman/dosya_yapisi.md](teknik_dokuman/dosya_yapisi.md): Repo klasörleri ve önemli kaynak dosyaların kısa rehberi
- [teknik_dokuman/topoloji.md](teknik_dokuman/topoloji.md): Sistem topolojisi (Raspi–PC–Arduino), iş parçacıkları ve veri akışı
- [teknik_dokuman/protokoller.md](teknik_dokuman/protokoller.md): UDP video (SNK1) ve TCP telemetri (SNK2) protokolleri + çerçeveleme
- [teknik_dokuman/combat_pipeline.md](teknik_dokuman/combat_pipeline.md): Raspi “combat loop” akışı (kamera → tespit → takip → nişan → tetik)
- [teknik_dokuman/algoritmalar.md](teknik_dokuman/algoritmalar.md): Algoritmaların ayrıntıları (YOLO/HSV, segmentasyon, tracker, mesafe, balistik, state machine)
- [teknik_dokuman/konfigurasyon.md](teknik_dokuman/konfigurasyon.md): sancak_config.yml alanları + bilinen borçlar/dikkat noktaları
- [teknik_dokuman/checklist.md](teknik_dokuman/checklist.md): Saha kalibrasyonu ve emniyet checklist’i (HSV, LUT, geofence, fail-safe)
- [teknik_dokuman/test_senaryolari.md](teknik_dokuman/test_senaryolari.md): Operasyonel test senaryoları (PC bağlantı, overlay, SNK1/SNK2, fail-safe)
- [teknik_dokuman/komutlar.md](teknik_dokuman/komutlar.md): Hızlı build komutları (PC/Raspi)
- [teknik_dokuman/akis.md](teknik_dokuman/akis.md): Mermaid akış diyagramı

Notlar:
- Eski tek-parça doküman [TEKNIK_DOKUMAN.md](TEKNIK_DOKUMAN.md) olarak repoda duruyorsa, referans amaçlıdır; güncel okuma için bu klasördeki tematik dosyalar tercih edilmeli.
