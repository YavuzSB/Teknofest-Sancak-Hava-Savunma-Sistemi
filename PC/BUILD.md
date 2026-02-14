# Teknofest Sancak GCS – Derleme ve Dağıtım Rehberi

## Mimari

| Bileşen | Teknoloji |
|---------|-----------|
| UI Framework | Dear ImGui (C++17, immediate-mode) |
| Pencere / Girdi | GLFW 3.4 |
| Grafik | OpenGL 3.0 |
| JPEG Decode | stb_image (header-only) |
| Ağ (Video) | Winsock2 UDP |
| Ağ (Telemetri) | Winsock2 TCP |
| Build Sistemi | CMake 3.21+ / Ninja |
| Hedef | Windows 10/11 x64, tek .exe |

Tüm bağımlılıklar **FetchContent** ile otomatik indirilir; harici kurulum gerekmez.

---

## Gereksinimler (Bir Kere Kurulur)

| Araç | İndirme |
|------|---------|
| **Visual Studio 2022** (veya Build Tools) | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake ≥ 3.21** | [cmake.org](https://cmake.org/download/) |
| **Ninja** | [github.com/ninja-build/ninja](https://github.com/ninja-build/ninja/releases) |
| **Git** | [git-scm.com](https://git-scm.com/) |

> **Not:** Visual Studio kurulumunda "**C++ ile masaüstü geliştirme**" iş yükünü seçin.

---

## Derleme Adımları

### 1. Developer Command Prompt Açın

Başlat menüsünden **"x64 Native Tools Command Prompt for VS 2022"** açın.

### 2. Proje Klasörüne Gidin

```cmd
cd D:\code\Teknofest-Sancak-Hava-Savunma-Sistemi-master\PC
```

### 3. CMake Yapılandırma (İlk seferde bağımlılıklar indirilir)

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

**Debug derlemesi** (konsol çıktısı ve hata ayıklama):
```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTEKNOFEST_CONSOLE=ON
```

### 4. Derleme

```cmd
cmake --build build --config Release
```

### 5. Çalıştırma

```cmd
build\TeknofestGCS.exe
```

---

## Ortam Değişkenleri (İsteğe Bağlı)

| Değişken | Varsayılan | Açıklama |
|----------|-----------|----------|
| `RASPI_HOST` | `127.0.0.1` | Raspberry Pi IP adresi |
| `VIDEO_UDP_PORT` | `5005` | Video UDP portu |
| `TEL_TCP_PORT` | `5000` | Telemetri TCP portu |
| `ARDUINO_PORT` | *(boş)* | Arduino seri port (ör: `COM3`) |

Örnek:
```cmd
set RASPI_HOST=192.168.1.100
set VIDEO_UDP_PORT=5005
build\TeknofestGCS.exe
```

---

## Dağıtım (Tek .exe)

Statik runtime (`/MT`) ile derlendiği için çıkan `TeknofestGCS.exe` dosyası
harici DLL gerektirmez. Herhangi bir Windows 10/11 x64 bilgisayara
kopyalayarak doğrudan çalıştırabilirsiniz.

### Konsol Penceresini Kapatmak (Release)

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DTEKNOFEST_CONSOLE=OFF
cmake --build build --config Release
```

Bu modda `.exe` dosyası konsol penceresi açmadan sadece GUI görüntüler.

---

## Dosya Yapısı

```
PC/
├── CMakeLists.txt          # Build sistemi
├── BUILD.md                # Bu dosya
├── .clang-format           # Kod formatlama kuralları
├── icon.png                # Uygulama ikonu
├── include/
│   ├── App.hpp             # Ana uygulama sınıfı
│   ├── VideoReceiver.hpp   # UDP video alıcı
│   ├── TelemetryClient.hpp # TCP telemetri istemcisi
│   ├── ArduinoController.hpp # Seri port kontrolcü
│   └── TextureHelper.hpp   # OpenGL texture yardımcısı
└── src/
    ├── main.cpp            # Giriş noktası (GLFW + ImGui init)
    ├── App.cpp             # UI render, iş mantığı
    ├── VideoReceiver.cpp   # Winsock2 UDP + stb_image
    ├── TelemetryClient.cpp # Winsock2 TCP
    ├── ArduinoController.cpp # Seri port stub
    └── TextureHelper.cpp   # GL texture işlemleri
```

---

## Klavye Kısayolları

| Tuş | Komut |
|-----|-------|
| W | İleri hareket |
| A | Sola hareket |
| S | Geri hareket |
| D | Sağa hareket |
