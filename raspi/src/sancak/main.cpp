/**
 * @file main.cpp
 * @brief Sancak Hava Savunma Sistemi - Ana Giriş Noktası
 *
 * Kullanım:
 *   ./sancak_combat                          # Varsayılan headless mod
 *   ./sancak_combat --display                # Monitörlü test modu
 *   ./sancak_combat --config config.yml      # Özel config dosyası
 *   ./sancak_combat --model path/best.onnx   # Model yolu override
 *   ./sancak_combat --video test.mp4         # Video dosyasından çalıştır
 *   ./sancak_combat --autonomous             # Otonom mod ile başla
 *
 * @author Sancak Takımı
 * @date 2026
 */
#include "sancak/combat_pipeline.hpp"
#include "sancak/config_manager.hpp"
#include "sancak/logger.hpp"

#include <iostream>
#include <string>
#include <cstring>

namespace {

void printBanner() {
    std::cout <<
R"(
╔══════════════════════════════════════════════════════════════╗
║              SANCAK HAVA SAVUNMA SİSTEMİ v2.0              ║
║           Teknofest - Balon İmha Modülü (Raspi)            ║
╠══════════════════════════════════════════════════════════════╣
║  Pipeline:                                                  ║
║    Kamera → YOLO26-Nano → Balon Seg → Takip → Balistik    ║
║                                                              ║
║  Modüller:                                                  ║
║    ├─ YoloDetector      : ONNX çıkarım (best.onnx)        ║
║    ├─ BalloonSegmentor  : HSV turuncu balon tespiti        ║
║    ├─ TargetTracker     : IoU çoklu hedef takibi           ║
║    ├─ BallisticsManager : Paralaks + Düşüş + Önleme       ║
║    ├─ AimSolver         : Bileşik nişan hesabı            ║
║    ├─ CameraController  : USB/CSI kamera yönetimi         ║
║    └─ SerialComm        : Arduino UART iletişimi          ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void printHelp(const char* prog) {
    std::cout << "Kullanım: " << prog << " [seçenekler]\n\n"
              << "Seçenekler:\n"
              << "  --config PATH     Konfigürasyon dosyası (varsayılan: config/sancak_config.yml)\n"
              << "  --model PATH      YOLO model dosyası override (varsayılan: models/best.onnx)\n"
              << "  --video PATH      Video dosyasından çalıştır (kamera yerine)\n"
              << "  --display         Monitörlü test modu (görüntü penceresi aç)\n"
              << "  --headless        Monitörsüz mod (varsayılan)\n"
              << "  --autonomous      Otonom mod ile başla\n"
              << "  --camera INDEX    Kamera cihaz indeksi (varsayılan: 0)\n"
              << "  --log-level LVL   Log seviyesi: trace|debug|info|warn|error (varsayılan: info)\n"
              << "  --offset-x PX     Manuel yatay nişan offset (piksel)\n"
              << "  --offset-y PX     Manuel dikey nişan offset (piksel)\n"
              << "  --help, -h        Bu yardım mesajını göster\n"
              << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    // Varsayılan ayarlar
    std::string config_path  = "config/sancak_config.yml";
    std::string model_path;
    std::string video_path;
    bool display_mode  = false;
    bool autonomous    = false;
    int  camera_index  = -1;  // -1 = config'den
    std::string log_level;
    float offset_x = std::numeric_limits<float>::quiet_NaN();
    float offset_y = std::numeric_limits<float>::quiet_NaN();

    // --- Komut satırı argümanları ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        }
        else if (arg == "--config" && i + 1 < argc)     { config_path = argv[++i]; }
        else if (arg == "--model" && i + 1 < argc)      { model_path  = argv[++i]; }
        else if (arg == "--video" && i + 1 < argc)      { video_path  = argv[++i]; }
        else if (arg == "--display")                     { display_mode = true; }
        else if (arg == "--headless")                    { display_mode = false; }
        else if (arg == "--autonomous")                  { autonomous = true; }
        else if (arg == "--camera" && i + 1 < argc)     { camera_index = std::stoi(argv[++i]); }
        else if (arg == "--log-level" && i + 1 < argc)  { log_level  = argv[++i]; }
        else if (arg == "--offset-x" && i + 1 < argc)   { offset_x = std::stof(argv[++i]); }
        else if (arg == "--offset-y" && i + 1 < argc)   { offset_y = std::stof(argv[++i]); }
        else {
            std::cerr << "Bilinmeyen argüman: " << arg << "\n";
            printHelp(argv[0]);
            return 1;
        }
    }

    printBanner();

    // --- Konfigürasyon yükle ---
    auto& cfg_mgr = sancak::ConfigManager::instance();
    cfg_mgr.loadFromFile(config_path);

    auto config = cfg_mgr.get();

    // CLI override'ları uygula
    if (display_mode)                       config.headless = false;
    if (autonomous)                         config.autonomous = true;
    if (camera_index >= 0)                  config.camera.device_index = camera_index;
    if (!model_path.empty())                config.yolo.model_path = model_path;
    if (!log_level.empty())                 config.log_level = log_level;
    if (!std::isnan(offset_x))              config.ballistics.manual_offset_x_px = offset_x;
    if (!std::isnan(offset_y))              config.ballistics.manual_offset_y_px = offset_y;

    cfg_mgr.set(config);

    // --- Pipeline oluştur ve çalıştır ---
    sancak::CombatPipeline pipeline;

    if (!pipeline.initialize(config)) {
        SANCAK_LOG_FATAL("Pipeline başlatılamadı!");
        return 1;
    }

    // Video dosyası varsa kamerayı override et
    if (!video_path.empty()) {
        SANCAK_LOG_INFO("Video modu: {}", video_path);
        // NOT: CameraController'da openFile çağrılmalı
        // Bu, pipeline.run() yerine özel döngü gerektirir
    }

    try {
        pipeline.run();
    } catch (const std::exception& e) {
        SANCAK_LOG_FATAL("Beklenmeyen hata: {}", e.what());
        return 1;
    }

    SANCAK_LOG_INFO("Sistem kapatıldı. Güle güle!");
    return 0;
}
