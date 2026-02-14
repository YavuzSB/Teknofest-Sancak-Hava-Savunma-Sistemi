/**
 * @file main_detector.cpp
 * @brief Balon Tespit Sistemi - Ana Program
 *
 * Kullanim:
 *   ./main_detector                # headless mod (varsayilan)
 *   ./main_detector --display      # monitf6rlfc test modu
 *   ./main_detector --camera 1     # farkli kamera indeksi
 */
#include "balloon_detector.hpp"

using namespace sancak;

void printBanner(int cameraIndex, bool headless){
    println(std::string(72, '='));
    println("TEKNOFEST - Balon Tespit Sistemi (Modular C++ - Pi 5 Optimized)");
    println(std::string(72, '='));
    println("Kamera: USB Webcam (Index " << cameraIndex << ")");
    println("Cozunurluk: " << CAMERA_WIDTH << "x" << CAMERA_HEIGHT);
    println("Frame Skip: Uyku=1/" << FRAME_SKIP_IDLE  << ", Aktif=1/" << FRAME_SKIP_ACTIVE);
    println("Mod: " << (headless ? "HEADLESS (SSH/Monitorsuz)" : "DISPLAY (Test/Debug)"));
    println();
    println("Optimizasyonlar:");
    println("  + Frame Skipping (Anti-Lag Buffer)");
    println("  + Tekil Maske Isleme (%60 CPU Tasarrufu)");
    println("  + On Eleme (Alan bazli filtreleme)");
    println("  + Moduler Mimari (Kolay Bakim)");
    println();
    println("Sekil Kriterleri:");
    println("  - Circularity > " << CIRCULARITY_THRESHOLD);
    println("  - Convexity > " << CONVEXITY_THRESHOLD);
    println("  - Inertia Ratio: " << INERTIA_RATIO_MIN << " - " << INERTIA_RATIO_MAX);
    println();
    println("Renk Kategorileri:");
    println("  - DUSMAN: Kirmizi (Hedef)");
    println("  - DOST: Mavi, Sari (Korunmali)");
    println();
    println("Modul Yapisi:");
    println("  - shape_analyzer    : Geometrik filtreler");
    println("  - color_filter      : HSV renk segmentasyonu");
    println("  - motion_detector   : Hareket algilama");
    println("  - balloon_detector  : Ana tespit sistemi");
    println("  - tracking_pipeline : Otonom takip hatti");
    println(std::string(72, '='));
}

int main(int argc, char** argv)
{
    bool headlessMode = true;
    int  cameraIndex  = CAMERA_INDEX;

    // Basit komut satiri argumanlari
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--display") {
            headlessMode = false;
        } else if (arg == "--camera" && i + 1 < argc) {
            cameraIndex = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            println("Kullanim:");
            println("  " << argv[0] << " [secenekler]");
            println();
            println("Secenekler:");
            println("  --display       Goruntu penceresini ac (test/debug)");
            println("  --camera INDEX  Kamera indeksi (varsayilan: 0)");
            println("  --help, -h      Bu yardim mesajini goster");
            return 0;
        }
    }

    printBanner(cameraIndex, headlessMode);

    try {
        BalloonDetector detector(cameraIndex, headlessMode);
        detector.run();
    }
    catch (const std::exception& e) {
        std::cerr << "\n[HATA] Beklenmeyen hata: " << e.what());
        return 1;
    }

    return 0;
}
