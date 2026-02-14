Teknofest Raspberry Pi Yazılımı (Modüler Mimari)
===============================================

Bu README dosyasi agirlikla eski/legacy Python akisina aittir.
Guncel C++17 + OpenCV moduler sistem ve build/test akisi icin:
- docs/README_MODULAR.md

Bileşenler
----------
1. vision_system.py
   - `DetectionPipeline` sınıfı
   - Hibrit durum makinesi: IDLE -> ROI -> TRACK
   - Otonom modda hedef (balon) koordinatı üretir, manuel modda ham görüntü gönderir.

2. hardware_input.py
   - `GamepadGPIO` sınıfı
   - Fiziksel butonlar: forward, back, left, right, fire, mode
   - Debounce mekanizması ve callback desteği
   - RPi.GPIO yoksa mock mod

3. main_pi.py
   - Konfigürasyon yönetimi (HSV)
   - Arduino seri haberleşmesi (mock fallback)
   - UDP video yayını (640x480 JPEG) -> PC (192.168.1.10:5005)
   - TCP komut sunucusu (0.0.0.0:5000) <SET>, <MOVE>, <CMD> komutları
   - Gamepad poller thread + Kamera thread + TCP thread
   - Mod mantığı: Mode butonu veya TCP <CMD:START>/<CMD:STOP>

Çalıştırma
----------
Ön koşullar:
```
pip install -r requirements.txt
```

Çalıştır:
```
python3 main_pi.py
```

Komut Formatları (TCP)
----------------------
```
<SET:H_MIN,10>
<CMD:SAVE_CALIB>
<MOVE:FORWARD>
<CMD:START>  # otonom aç
<CMD:STOP>   # otonom kapat
```

Hedef Mesajı (Arduino)
----------------------
Otonom modda: `TARGET:err_x,err_y` (frame merkezine göre hata)
Manuel modda: `MOVE:W|A|S|D` buton basıldığında
Her modda Ateş: `FIRE`

Notlar
------
- Gerçek Pi üzerinde performans için `opencv-python-headless` tercih edilebilir.
- RPi.GPIO sadece gerçek donanımda çalışır; geliştirme ortamında mock mod.
- HSV kalibrasyonu `config.json` dosyasında saklanır.

C++ Balon Tespit (PC Webcam Test)
-------------------------------
Moduler C++ balon tespit kodunu PC uzerinde webcam ile calistirmak icin `pc_webcam_test` hedefini kullanin.
Detaylar: `docs/README_MODULAR.md`
