# SANCAK Balon Tespit Sistemi - Moduler C++ Yapisi

Teknofest Hava Savunma Sistemi icin gelistirilmis, yuksek performansli ve moduler balon tespit sistemi.

## Moduler Mimari

### Temel Moduller

| Modul | Dosyalar | Sorumluluk |
|-------|----------|------------|
| **shape_analyzer** | `include/shape_analyzer.hpp`, `src/shape_analyzer.cpp` | Geometrik sekil analizi (circularity, convexity, inertia) |
| **color_filter** | `include/color_filter.hpp`, `src/color_filter.cpp` | HSV renk segmentasyonu ve maske islemleri |
| **motion_detector** | `include/motion_detector.hpp`, `src/motion_detector.cpp` | Frame difference bazli hareket algilama |
| **tracking_pipeline** | `include/tracking_pipeline.hpp`, `src/tracking_pipeline.cpp` | IDLE->ROI->TRACK durum makinesi (otonom takip) |
| **balloon_detector** | `include/balloon_detector.hpp`, `src/balloon_detector.cpp` | Ana tespit sistemi (frame skipping, coklu renk) |

### Ana Programlar
- `src/main_detector.cpp` : Modul yapinin ana ornek programi
- `src/detect_balloons.cpp` : Eski monolitik ornek (uyumluluk icin)
- `tests/pc_webcam_test.cpp` : PC uzerinde USB webcam/facecam ile ayni C++ kodu test etmek icin

### Eski Dosyalar (Geriye Donuk Uyumluluk)
- `include/vision_system.hpp`, `src/vision_system.cpp` -> `tracking_pipeline` ile degistirildi
- `include/detect_balloons.hpp`, `src/detect_balloons.cpp` -> moduler yapiya gecirildi

---

## Derleme ve Kurulum (Raspberry Pi 5 icin onerilen)

### Gereksinimler
```bash
sudo apt install build-essential cmake
sudo apt install libopencv-dev  # OpenCV 4.x
```

### Derleme
```bash
cd teknofest
cmake --preset linux-ninja-release
cmake --build --preset linux-ninja-release
ctest --preset linux-ninja-release
```

---

## Windows uzerinde derleme

Bu repo Windows'ta su yaklasimlarla rahat gelistirilir:
- MSVC (Visual Studio 2022) + CMake Presets (onerilen)
- MSYS2 (MinGW UCRT64) (alternatif)

### Windows (MSVC) - CMake Presets (onerilen)

On kosullar:
- Visual Studio 2022 (Desktop development with C++)
- CMake (3.21+)
- OpenCV (MSVC ile uyumlu kurulum)

Derleme (VS Developer PowerShell/Prompt icinden):
```powershell
cd teknofest
cmake --preset win-ninja-msvc-debug
cmake --build --preset win-ninja-msvc-debug
ctest --preset win-ninja-msvc-debug
```

Notlar:
- Runner hedefleri varsayilan acik: `SANCAK_BUILD_RUNNERS=ON`
- Legacy hedefler varsayilan kapali: `SANCAK_BUILD_LEGACY=OFF`
- OpenCV bulunamiyorsa `CMAKE_PREFIX_PATH` (veya vcpkg) ile konumunu belirtin.

### Windows (MSYS2 - MinGW UCRT64) (alternatif)

Bu repo C++ tarafinda OpenCV kullandigi icin Windows'ta once OpenCV'yi ayni toolchain ile kurmak gerekir.

1) MSYS2 UCRT64 terminalinde (Start Menu -> "MSYS2 UCRT64"):
```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-opencv
```

2) PowerShell uzerinden (CMake Presets ile) configure/build/test (onerilen):
```powershell
cd teknofest
cmake --preset win-mingw-ucrt64-debug
cmake --build --preset win-mingw-ucrt64-debug --parallel 2
ctest --preset win-mingw-ucrt64-debug
```

Fallback (manuel configure/build) ornegi:
```powershell
cmake -S raspi -B raspi/build-mingw -G "MinGW Makefiles" `
	-DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/mingw32-make.exe `
	-DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
	-DCMAKE_PREFIX_PATH=C:/msys64/ucrt64

cmake --build raspi/build-mingw --parallel
```

### Kurulum (opsiyonel)
```bash
sudo make install
```

Calistirilabilirler `bin/` altina kurulur:
- `main_detector`
- `detect_balloons`
- `pc_webcam_test`

---

## Kullanim

### Temel Kullanim (moduler ana program)
```bash
# Headless mod (SSH/monitorsuz - varsayilan)
./main_detector

# Goruntulu test modu
./main_detector --display

# Farkli kamera indeksi
./main_detector --camera 1
```

### PC uzerinde webcam ile test (onerilen gelistirme akisi)
Bu test, yeni bir algoritma yazmaz; dogrudan var olan `BalloonDetector` sinifini calistirir.
Bu sayede ana kod degistikce test dosyasini da ayri ayri guncellemek gerekmez.

Derleme:
```bash
cd raspi
cmake -S . -B build
cmake --build build --config Release
```

Calistirma:
```bash
# Varsayilan: goruntulu test
./build/pc_webcam_test --camera 0

# Headless istersen
./build/pc_webcam_test --camera 0 --headless

# Webcam yerine video dosyasi ile (Docker/Windows passthrough kisitlari icin)
./build/pc_webcam_test --video path/to/video.mp4
```

---

## Testler (Unit + Opsiyonel Video Fixture)

Unit testler tamamen sentetik frame'lerle kosar (kamera gerekmez).

Opsiyonel olarak bir video dosyasi ile "crash etmeme / basic pipeline saglamligi" entegrasyon testi de vardir.
Bu test varsayilan kapali gelir (Windows'ta OpenCV videoio DLL bagimliliklari sorun cikarmasin diye).

Secenek 1 (CMake cache ile):
```bash
cmake --preset linux-ninja-release -DSANCAK_ENABLE_INTEGRATION_TESTS=ON -DSANCAK_TEST_VIDEO_PATH=/abs/path/to/video.mp4
cmake --build --preset linux-ninja-release
ctest --preset linux-ninja-release
```

Secenek 2 (env var ile):
```bash
cmake --preset linux-ninja-release -DSANCAK_ENABLE_INTEGRATION_TESTS=ON
cmake --build --preset linux-ninja-release
SANCAK_TEST_VIDEO=/abs/path/to/video.mp4 ctest --preset linux-ninja-release
```

### Eski monolitik ornek
```bash
./detect_balloons --display
```

Headless modda konsola FPS ve tespit sayilari yazilir.

---

## Modul Detaylari (Ozet)

### shape_analyzer
- Sekil kriterleri: circularity > 0.6, convexity > 0.85, inertia ratio 0.3 - 1.5
- Minimum alan: 500 piksel^2

### color_filter
- Kirmizi icin iki HSV araligi (0-10, 170-180)
- Mavi ve sari icin ayri araliklar
- Tekil maske olusturma ve morfolojik temizlik

### motion_detector
- Frame farki + blur + threshold + morph ile hareket algilar
- En buyuk konturu secip ROI dondurur

### tracking_pipeline
- IDLE: hareket bekler
- ROI: hareket bolgesinde HSV arama
- TRACK: hedef varsa takip, kaybolursa ROI/IDLE donusu

### balloon_detector
- Kamera acma, frame skipping, tum renklerin islenmesi
- Dusman (kirmizi) ve dost (mavi/sari) ayrimi

---

## Gelistirme Notlari

- Kodlar C++17 ile derlenir.
- Tum header dosyalari `include/`, kaynak dosyalar `src/` altindadir.
- OpenCV `find_package(OpenCV REQUIRED)` ile bulunur, CMake include yoluna eklenir.

Son guncelleme: 6 Subat 2026
