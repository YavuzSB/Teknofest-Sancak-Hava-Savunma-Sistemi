## 2) Derleme Sistemi ve Hedefler

### 2.1 Kök CMake (Wrapper)
- Dosya: `CMakeLists.txt`
- Amaç: CMake Presets vb. root’tan configure edilebilsin.
- Seçenekler:
  - `SANCAK_BUILD_RASPI` (Windows’ta varsayılan OFF, Linux’ta ON)
  - `SANCAK_BUILD_PC` (Windows’ta varsayılan ON, Linux’ta OFF)

Bu sayede Windows’ta OpenCV yokken bile PC/ kısmı derlenebilir.

### 2.2 Raspi tarafı (raspi/CMakeLists.txt)
- C++ standard: C++17
- Bağımlılık: OpenCV (core/imgproc/highgui/videoio/dnn)
- Hedefler:
  - sancak_network (UDP video + TCP telemetri + TelemetrySender)
  - sancak_config (OpenCV FileStorage ile config)
  - sancak_yolo, sancak_balloon, sancak_distance, sancak_ballistics, sancak_aim, …
  - sancak_combat (ana executable)
  - Testler: Catch2 (FetchContent) ile sancak_tests vb.

### 2.3 PC tarafı (PC/CMakeLists.txt)
- UI: Dear ImGui (docking branch)
- Window/Input: GLFW 3.4
- Render: OpenGL
- JPEG decode: stb_image
- Network: Winsock2 (Windows)
- FetchContent ile bağımlılıklar otomatik çekilir, hedef tek .exe.

---
