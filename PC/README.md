# Teknofest GCS - C++ Qt6 Implementation

Modern C++17 ile yazılmış profesyonel Teknofest Ground Control Station uygulaması.

## Özellikler

- ✅ Modern C++17 standartları
- ✅ Smart pointer kullanımı (unique_ptr, shared_ptr)
- ✅ RAII prensipleri
- ✅ Qt6 ile GUI
- ✅ Thread-safe UDP/TCP iletişimi
- ✅ PIMPL idiom (ArduinoController)
- ✅ Const correctness
- ✅ Copy/Move semantics kontrolü
- ✅ Profesyonel dosya yapısı

## Gereksinimler

- CMake 3.21+
- Qt6 (Core, Widgets, Network, Gui)
- C++17 uyumlu derleyici (MSVC 2019+, GCC 9+, Clang 10+)

## Derleme

### Windows (Visual Studio)

```powershell
cd PC
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Linux/macOS

```bash
cd PC
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Çalıştırma

### Ortam Değişkenleri

```powershell
# Raspberry Pi host adresi
$env:RASPI_HOST = "192.168.1.100"

# Video UDP portu
$env:VIDEO_UDP_PORT = "5005"

# Telemetri TCP portu
$env:TEL_TCP_PORT = "5000"

# Arduino seri portu (opsiyonel)
$env:ARDUINO_PORT = "COM3"

# Çalıştır
.\build\Release\TeknofestGCS.exe
```

## Mimari

### Sınıf Yapısı

```
teknofest::
  ├── MainWindow          - Ana pencere (QMainWindow)
  ├── VideoThread         - UDP video alıcı thread
  ├── TelemetryThread     - TCP telemetri/komut thread
  ├── CameraWidget        - Kamera görüntü widget
  └── ArduinoController   - Arduino seri port kontrolcüsü (PIMPL)
```

### Dosya Yapısı

```
PC/
├── CMakeLists.txt       - Ana CMake yapılandırması
├── README.md            - Bu dosya
├── BUILD.md             - Detaylı derleme talimatları
├── .clang-format        - Kod stil ayarları
├── include/             - Header dosyaları (.hpp)
│   ├── MainWindow.hpp
│   ├── VideoThread.hpp
│   ├── TelemetryThread.hpp
│   ├── CameraWidget.hpp
│   └── ArduinoController.hpp
├── src/                 - Kaynak dosyaları (.cpp)
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── VideoThread.cpp
│   ├── TelemetryThread.cpp
│   ├── CameraWidget.cpp
│   └── ArduinoController.cpp
└── resources/           - Qt kaynakları
    └── resources.qrc
```

## Özellikler

### Modern C++ Kullanımı

- **Smart Pointers**: Ham pointer yerine `std::unique_ptr` ve `std::shared_ptr`
- **RAII**: Kaynaklar otomatik yönetilir
- **Move Semantics**: Gereksiz kopyalama önlenir
- **Const Correctness**: Thread-safe ve güvenli kod
- **Auto**: Tür çıkarımı ile daha temiz kod

### Thread Safety

- `std::atomic` ile flag yönetimi
- `QMutex` ile veri paylaşımı koruması
- Signal/Slot ile thread-safe iletişim

### Qt Best Practices

- Signals/Slots ile loose coupling
- Parent-child widget yönetimi ile otomatik bellek temizliği
- QThread ile uygun thread kullanımı
- Qt naming conventions

## Geliştirme

### Kod Stili

- C++17 standartları
- Header guards yerine `#pragma once`
- Namespace kullanımı (`teknofest::`)
- Doxygen yorumları
- LLVM code style

### Test

TODO: Unit testler eklenecek (Google Test veya Qt Test)

## Lisans

Teknofest projesi kapsamında geliştirilmiştir.
