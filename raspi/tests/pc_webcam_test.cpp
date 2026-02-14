/**
 * @file pc_webcam_test.cpp
 * @brief PC uzerinde USB webcam/facecam ile ayni C++ balon tespit kodunu test etmek icin.
 *
 * Bu dosya yeni bir algoritma icermez; var olan BalloonDetector sinifini calistirir.
 * Boylece algoritma degistikce test de otomatik olarak ayni kodu kullanir.
 *
 * Kullanim:
 *   pc_webcam_test --camera 0
 *   pc_webcam_test --camera 0 --headless
 */

#include "balloon_detector.hpp"

#include <iostream>
#include <string>

using namespace sancak;

int main(int argc, char** argv)
{
    int cameraIndex = CAMERA_INDEX;
    std::string videoPath;
    bool headlessMode = false; // PC testinde varsayilan olarak goruntu acik

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--camera" && i + 1 < argc) {
            cameraIndex = std::stoi(argv[++i]);
        } else if (arg == "--video" && i + 1 < argc) {
            videoPath = argv[++i];
        } else if (arg == "--headless") {
            headlessMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Kullanim:\n"
                      << "  " << argv[0] << " [--camera INDEX] [--video PATH] [--headless]\n";
            return 0;
        }
    }

    try {
        BalloonDetector detector = videoPath.empty()
            ? BalloonDetector(cameraIndex, headlessMode)
            : BalloonDetector(videoPath, headlessMode);
        detector.run();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
