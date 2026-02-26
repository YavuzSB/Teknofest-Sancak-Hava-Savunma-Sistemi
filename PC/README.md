# Teknofest GCS (PC) — C++17 ImGui/GLFW/OpenGL Uygulaması

Modern C++17 ile yazılmış Teknofest Ground Control Station (GCS) uygulaması.

## Özellikler

- ✅ Modern C++17 standartları
- ✅ Smart pointer kullanımı (unique_ptr, shared_ptr)
- ✅ RAII prensipleri
- ✅ Dear ImGui ile GUI (immediate-mode)
- ✅ Thread-safe UDP/TCP iletişimi
- ✅ Const correctness
- ✅ Copy/Move semantics kontrolü
- ✅ Profesyonel dosya yapısı

## Gereksinimler

- CMake 3.21+
- C++17 uyumlu derleyici (MSVC 2019+, GCC 9+, Clang 10+)

> Not: Detaylı derleme/dağıtım adımları için [BUILD.md](BUILD.md) dosyasına bak.

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

### Bileşenler

- `App`: ImGui panel çizimi + UI iş akışı
- `VideoReceiver`: UDP video alıcı + (varsa) fragment reassembly + JPEG decode
- `TelemetryClient`: TCP telemetri istemcisi + parse + yeniden bağlanma
- `ArduinoController`: (PC tarafı) seri port/komut iletimi için kontrol katmanı
- `TextureHelper`: OpenGL texture yardımcıları

### Dosya Yapısı

```
PC/
├── CMakeLists.txt       - Ana CMake yapılandırması
├── README.md            - Bu dosya
├── BUILD.md             - Detaylı derleme talimatları
├── .clang-format        - Kod stil ayarları
├── include/             - Header dosyaları (.hpp)
│   ├── App.hpp
│   ├── ArduinoController.hpp
│   ├── CsvLogger.hpp
│   ├── NetworkProtocol.hpp
│   ├── TelemetryClient.hpp
│   ├── TextureHelper.hpp
│   └── VideoReceiver.hpp
├── src/                 - Kaynak dosyaları (.cpp)
│   ├── main.cpp
│   ├── App.cpp
│   ├── ArduinoController.cpp
│   ├── CsvLogger.cpp
│   ├── TelemetryClient.cpp
│   ├── TextureHelper.cpp
│   └── VideoReceiver.cpp
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
- `std::mutex` ile paylaşılan veri koruması (UI thread ↔ ağ thread’leri)

## Geliştirme

### Kod Stili

- C++17 standartları
- Header guards yerine `#pragma once`
- Namespace kullanımı (`teknofest::`)
- Doxygen yorumları
- LLVM code style

### Test

TODO: Unit testler eklenecek.

## Lisans

Teknofest projesi kapsamında geliştirilmiştir.
