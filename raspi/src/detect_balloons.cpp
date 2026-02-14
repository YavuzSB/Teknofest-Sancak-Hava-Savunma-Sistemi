/**
 * @file detect_balloons.cpp
 * @brief Gelişmiş Balon Tespit Sistemi - Implementasyon (Pi 5 Optimized)
 */
#include "detect_balloons.hpp"
#include <thread>

using namespace cv;
using namespace std;
using Clock = chrono::steady_clock;

// ========================================================================
// HSV Renk Aralıkları (Sabitler)
// ========================================================================
// Kırmızı (HSV'de 0 ve 180 civarı)
static const Scalar HSV_RED_LOW_L  (0,   100, 80);
static const Scalar HSV_RED_LOW_H  (10,  255, 255);
static const Scalar HSV_RED_HIGH_L (170, 100, 80);
static const Scalar HSV_RED_HIGH_H (180, 255, 255);

// Mavi
static const Scalar HSV_BLUE_L (100, 100, 70);
static const Scalar HSV_BLUE_H (130, 255, 255);

// Sarı
static const Scalar HSV_YELLOW_L (20,  100, 70);
static const Scalar HSV_YELLOW_H (35,  255, 255);


// ========================================================================
// ŞEKİL ANALİZİ FONKSİYONLARI
// ========================================================================

double calculateCircularity(const vector<Point>& contour, double area, double perimeter)
{
    if (perimeter == 0.0)
        return 0.0;
    double c = (4.0 * CV_PI * area) / (perimeter * perimeter);
    return min(c, 1.0);
}

double calculateConvexity(const vector<Point>& contour, double area)
{
    vector<Point> hull;
    convexHull(contour, hull);
    double hullArea = contourArea(hull);
    if (hullArea == 0.0)
        return 0.0;
    return min(area / hullArea, 1.0);
}

double calculateInertiaRatio(const vector<Point>& contour)
{
    if (contour.size() < 5)
        return 0.0;

    Moments m = moments(contour);
    if (m.m00 == 0.0)
        return 0.0;

    double mu20 = m.mu20 / m.m00;
    double mu02 = m.mu02 / m.m00;
    double mu11 = m.mu11 / m.m00;

    double common = sqrt(4.0 * mu11 * mu11 + (mu20 - mu02) * (mu20 - mu02));
    if (common == 0.0)
        return 1.0;

    double lambda1 = (mu20 + mu02 + common) / 2.0;
    double lambda2 = (mu20 + mu02 - common) / 2.0;

    if (lambda2 == 0.0)
        return 0.0;

    return lambda1 / lambda2;
}

BalloonShapeResult isBalloonShape(const vector<Point>& contour)
{
    BalloonShapeResult result;

    double area      = contourArea(contour);
    double perimeter = arcLength(contour, true);

    if (area < MIN_CONTOUR_AREA || perimeter == 0.0)
        return result;   // isBalloon = false

    double circularity = calculateCircularity(contour, area, perimeter);
    double convexity   = calculateConvexity(contour, area);
    double inertia     = calculateInertiaRatio(contour);

    result.debug.area        = static_cast<int>(area);
    result.debug.circularity = circularity;
    result.debug.convexity   = convexity;
    result.debug.inertia     = inertia;

    bool isCircular    = circularity > CIRCULARITY_THRESHOLD;
    bool isConvex      = convexity   > CONVEXITY_THRESHOLD;
    bool isProperRatio = (inertia > INERTIA_RATIO_MIN) && (inertia < INERTIA_RATIO_MAX);

    result.isBalloon = isCircular && isConvex && isProperRatio;

    if (result.isBalloon) {
        double circScore    = min(circularity, 1.0) * 0.4;
        double convScore    = min(convexity,   1.0) * 0.4;
        double inertiaScore = (1.0 - abs(1.0 - inertia)) * 0.2;
        result.confidence   = circScore + convScore + inertiaScore;
    }

    return result;
}


// ========================================================================
// GÖRÜNTÜ İŞLEME YARDIMCILARI
// ========================================================================

Mat applyMorphology(const Mat& mask, int kernelSize, int iterations)
{
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));

    Mat out;
    // Opening: küçük gürültüleri temizler
    morphologyEx(mask, out, MORPH_OPEN, kernel, Point(-1, -1), 1);
    // Closing: delikleri kapatır
    morphologyEx(out, out, MORPH_CLOSE, kernel, Point(-1, -1), iterations);

    return out;
}

bool detectMotion(const Mat& currentGray, const Mat& previousGray, int threshold)
{
    if (previousGray.empty())
        return false;

    Mat frameDiff, thresh;
    absdiff(currentGray, previousGray, frameDiff);
    cv::threshold(frameDiff, thresh, 25, 255, THRESH_BINARY);

    double totalDiff = sum(thresh)[0];
    return totalDiff > threshold;
}


// ========================================================================
// AdvancedBalloonDetector  –  Constructor / InitCamera
// ========================================================================

AdvancedBalloonDetector::AdvancedBalloonDetector(int cameraIndex, bool headless)
    : cameraIndex_(cameraIndex)
    , headless_(headless)
    , isActiveMode_(false)
    , frameCount_(0)
    , processCount_(0)
    , fps_(0.0)
{
    lastTime_ = Clock::now();
}

void AdvancedBalloonDetector::initializeCamera()
{
    cout << "[INFO] Kamera baslatiliyor: Index " << cameraIndex_ << endl;

    // Pi 5'te V4L2 backend tercih
    cap_.open(cameraIndex_, CAP_V4L2);
    if (!cap_.isOpened()) {
        cout << "[WARNING] V4L2 basarisiz, varsayilan backend deneniyor..." << endl;
        cap_.open(cameraIndex_);
        if (!cap_.isOpened())
            throw runtime_error("Kamera acilamadi: Index " + to_string(cameraIndex_));
    }
    cout << "[INFO] Kamera acildi" << endl;

    cap_.set(CAP_PROP_FRAME_WIDTH,  CAMERA_WIDTH);
    cap_.set(CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT);
    cap_.set(CAP_PROP_FPS, 30);
    cap_.set(CAP_PROP_AUTOFOCUS, 0);
    cap_.set(CAP_PROP_AUTO_EXPOSURE, 1);   // Manuel
    cap_.set(CAP_PROP_BUFFERSIZE, 1);

    cout << "[INFO] Kamera hazir: "
         << static_cast<int>(cap_.get(CAP_PROP_FRAME_WIDTH)) << "x"
         << static_cast<int>(cap_.get(CAP_PROP_FRAME_HEIGHT)) << endl;
}


// ========================================================================
// processAllColors  –  Tekil Maske İşleme
// ========================================================================

DetectionResult AdvancedBalloonDetector::processAllColors(const Mat& frame)
{
    // HSV'ye dönüştür (TEK SEFER)
    Mat hsv, hsvBlur;
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    GaussianBlur(hsv, hsvBlur, Size(5, 5), 0);

    // --- Renk maskeleri ---
    Mat maskRed1, maskRed2, maskRed;
    inRange(hsvBlur, HSV_RED_LOW_L,  HSV_RED_LOW_H,  maskRed1);
    inRange(hsvBlur, HSV_RED_HIGH_L, HSV_RED_HIGH_H, maskRed2);
    bitwise_or(maskRed1, maskRed2, maskRed);

    Mat maskBlue;
    inRange(hsvBlur, HSV_BLUE_L, HSV_BLUE_H, maskBlue);

    Mat maskYellow;
    inRange(hsvBlur, HSV_YELLOW_L, HSV_YELLOW_H, maskYellow);

    // Dost maskesi (Mavi + Sarı)
    Mat maskFriend;
    bitwise_or(maskBlue, maskYellow, maskFriend);

    // Birleşik mask (tüm renkler)
    Mat maskCombined;
    bitwise_or(maskRed, maskFriend, maskCombined);

    // Morfoloji (tek sefer)
    maskCombined = applyMorphology(maskCombined);

    // TEK SEFERDE kontur bul
    vector<vector<Point>> contours;
    findContours(maskCombined, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    DetectionResult result;

    for (const auto& contour : contours) {
        // ÖN ELEME: küçük konturları atla
        double area = contourArea(contour);
        if (area < MIN_CONTOUR_AREA)
            continue;

        // Şekil analizi
        auto shape = isBalloonShape(contour);
        if (!shape.isBalloon)
            continue;

        Rect bbox = boundingRect(contour);
        int x = bbox.x, y = bbox.y, w = bbox.width, h = bbox.height;

        // ROI içindeki pikselleri analiz et — hangi renk?
        Mat roiRed    = maskRed(bbox);
        Mat roiFriend = maskFriend(bbox);

        int redPixels    = countNonZero(roiRed);
        int friendPixels = countNonZero(roiFriend);

        BalloonDetection det;
        det.bbox       = bbox;
        det.confidence = shape.confidence;
        det.debug      = shape.debug;

        if (redPixels > friendPixels) {
            det.colorName = "red";
            result.enemy.push_back(det);
        } else {
            // Mavi mi sarı mı?
            Mat roiBlue   = maskBlue(bbox);
            Mat roiYellow = maskYellow(bbox);
            int bluePx  = countNonZero(roiBlue);
            int yellowPx = countNonZero(roiYellow);
            det.colorName = (bluePx > yellowPx) ? "blue" : "yellow";
            result.friendd.push_back(det);
        }
    }

    return result;
}


// ========================================================================
// drawDetections
// ========================================================================

void AdvancedBalloonDetector::drawDetections(Mat& frame,
                                              const vector<BalloonDetection>& detections,
                                              bool isEnemy)
{
    Scalar boxColor = isEnemy ? COLOR_RED_BGR : COLOR_GREEN_BGR;

    for (const auto& d : detections) {
        rectangle(frame, d.bbox, boxColor, 2);

        string labelType = isEnemy ? "DUSMAN" : "DOST";
        char buf[64];
        snprintf(buf, sizeof(buf), "%s (Conf: %.2f)", labelType.c_str(), d.confidence);
        string label(buf);

        int baseline = 0;
        Size textSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        rectangle(frame,
                  Point(d.bbox.x, d.bbox.y - textSize.height - 10),
                  Point(d.bbox.x + textSize.width, d.bbox.y),
                  boxColor, FILLED);
        putText(frame, label, Point(d.bbox.x, d.bbox.y - 5),
                FONT_HERSHEY_SIMPLEX, 0.6, COLOR_WHITE_BGR, 2);

        // Debug bilgisi
        snprintf(buf, sizeof(buf), "A:%d C:%.2f Cv:%.2f",
                 d.debug.area, d.debug.circularity, d.debug.convexity);
        putText(frame, string(buf),
                Point(d.bbox.x, d.bbox.y + d.bbox.height + 20),
                FONT_HERSHEY_SIMPLEX, 0.4, boxColor, 1);
    }
}


// ========================================================================
// updateFps
// ========================================================================

double AdvancedBalloonDetector::updateFps()
{
    auto now = Clock::now();
    double elapsed = chrono::duration<double>(now - lastTime_).count();
    if (elapsed > 0.0)
        fps_ = 1.0 / elapsed;
    lastTime_ = now;
    return fps_;
}


// ========================================================================
// run  –  Ana Döngü
// ========================================================================

void AdvancedBalloonDetector::run()
{
    initializeCamera();

    cout << "[INFO] Sistem baslatildi. Cikmak icin 'q' tusuna basin." << endl;
    cout << "[INFO] Frame Skipping: Uyku=1/" << FRAME_SKIP_IDLE
         << ", Aktif=1/" << FRAME_SKIP_ACTIVE << endl;

    if (headless_)
        cout << "[INFO] HEADLESS MODE: Goruntu penceresi KAPALI" << endl;
    else
        cout << "[INFO] DISPLAY MODE: Goruntu penceresi ACIK" << endl;

    const string windowName = "TEKNOFEST - Balon Tespit (Pi 5 Optimized)";
    if (!headless_)
        namedWindow(windowName, WINDOW_NORMAL);

    Mat frame;
    while (true) {
        if (!cap_.read(frame)) {
            cout << "[HATA] Frame okunamadi!" << endl;
            break;
        }

        frameCount_++;

        // Frame atlama mantığı
        int frameSkip    = isActiveMode_ ? FRAME_SKIP_ACTIVE : FRAME_SKIP_IDLE;
        bool shouldProcess = (frameCount_ % frameSkip == 0);

        if (shouldProcess) {
            processCount_++;

            // Gri tonlama
            Mat gray;
            cvtColor(frame, gray, COLOR_BGR2GRAY);

            // Hareket algılama
            bool hasMotion = detectMotion(gray, previousGray_);
            previousGray_ = gray.clone();

            isActiveMode_ = hasMotion;

            if (isActiveMode_) {
                lastDetections_ = processAllColors(frame);
            }
        }

        // Mod bilgisi
        Scalar modeColor;
        string modeText;
        if (isActiveMode_) {
            modeText  = "MOD: TAKIP";
            modeColor = COLOR_RED_BGR;
        } else {
            modeText  = "MOD: UYKU";
            modeColor = COLOR_GREEN_BGR;
        }

        // Son tespitleri çiz
        drawDetections(frame, lastDetections_.enemy,  true);
        drawDetections(frame, lastDetections_.friendd, false);

        // FPS
        double currentFps = updateFps();

        // Overlay
        int infoY = 30;
        putText(frame, modeText, Point(10, infoY),
                FONT_HERSHEY_SIMPLEX, 0.8, modeColor, 2);

        infoY += 35;
        char fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", currentFps);
        putText(frame, fpsBuf, Point(10, infoY),
                FONT_HERSHEY_SIMPLEX, 0.6, COLOR_WHITE_BGR, 2);

        infoY += 30;
        string skipText = "Skip: 1/" + to_string(frameSkip);
        putText(frame, skipText, Point(10, infoY),
                FONT_HERSHEY_SIMPLEX, 0.5, COLOR_WHITE_BGR, 1);

        // Headless konsol çıktısı
        if (headless_ && shouldProcess) {
            cout << "[Frame " << frameCount_ << "] " << modeText
                 << " | FPS:" << currentFps
                 << " | Dusman:" << lastDetections_.enemy.size()
                 << " | Dost:" << lastDetections_.friendd.size() << endl;
        }

        // Görüntü göster (sadece display modda)
        if (!headless_) {
            imshow(windowName, frame);
            int key = waitKey(1) & 0xFF;
            if (key == 'q' || key == 27)     // 'q' veya ESC
                break;
        } else {
            // Headless modda kısa bekleme
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }

    cleanup();
}


// ========================================================================
// cleanup
// ========================================================================

void AdvancedBalloonDetector::cleanup()
{
    if (cap_.isOpened())
        cap_.release();
    destroyAllWindows();
    cout << "[INFO] Sistem kapatildi." << endl;
}


// ========================================================================
// main  –  Giriş Noktası
// ========================================================================

int main(int argc, char** argv)
{
    bool headlessMode = true;
    int  camIndex     = CAMERA_INDEX;

    // Basit argüman ayrıştırma
    for (int i = 1; i < argc; ++i) {
        string arg(argv[i]);
        if (arg == "--display")
            headlessMode = false;
        else if (arg == "--camera" && i + 1 < argc)
            camIndex = stoi(argv[++i]);
    }

    cout << string(70, '=') << endl;
    cout << "TEKNOFEST - Balon Tespit (Raspberry Pi 5 OPTIMIZED - C++)" << endl;
    cout << string(70, '=') << endl;
    cout << "Kamera: USB Webcam (Index " << camIndex << ")" << endl;
    cout << "Cozunurluk: " << CAMERA_WIDTH << "x" << CAMERA_HEIGHT << endl;
    cout << "Frame Skip: Uyku=1/" << FRAME_SKIP_IDLE
         << ", Aktif=1/" << FRAME_SKIP_ACTIVE << endl;
    cout << "Mod: " << (headlessMode ? "HEADLESS (SSH/Monitorsuz)" : "DISPLAY (Test/Debug)") << endl;
    cout << endl;
    cout << "Optimizasyonlar:" << endl;
    cout << "  + Frame Skipping (Anti-Lag Buffer)" << endl;
    cout << "  + Tekil Maske Isleme (%60 CPU Tasarrufu)" << endl;
    cout << "  + On Eleme (Alan bazli filtreleme)" << endl;
    cout << endl;
    cout << "Sekil Kriterleri:" << endl;
    cout << "  - Circularity > " << CIRCULARITY_THRESHOLD << endl;
    cout << "  - Convexity > " << CONVEXITY_THRESHOLD << endl;
    cout << "  - Inertia Ratio: " << INERTIA_RATIO_MIN << " - " << INERTIA_RATIO_MAX << endl;
    cout << endl;
    cout << "Renk Kategorileri:" << endl;
    cout << "  - DUSMAN: Kirmizi (Hedef)" << endl;
    cout << "  - DOST: Mavi, Sari (Korunmali)" << endl;
    cout << string(70, '=') << endl << endl;

    try {
        AdvancedBalloonDetector detector(camIndex, headlessMode);
        detector.run();
    }
    catch (const exception& e) {
        cerr << "\n[HATA] Beklenmeyen hata: " << e.what() << endl;
        return 1;
    }

    return 0;
}
