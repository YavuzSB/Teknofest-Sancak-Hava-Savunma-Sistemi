/**
 * @file shape_analyzer.cpp
 * @brief 5ekil Analizi Modfclfc - Implementasyon
 */
#include "shape_analyzer.hpp"

namespace sancak {

double calculateCircularity(const std::vector<cv::Point>& contour,
                            double area,
                            double perimeter)
{
    if (perimeter == 0.0)
        return 0.0;
    
    double c = (4.0 * CV_PI * area) / (perimeter * perimeter);
    return std::min(c, 1.0);
}

double calculateConvexity(const std::vector<cv::Point>& contour,
                         double area)
{
    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);
    
    double hullArea = cv::contourArea(hull);
    if (hullArea == 0.0)
        return 0.0;
    
    return std::min(area / hullArea, 1.0);
}

double calculateInertiaRatio(const std::vector<cv::Point>& contour)
{
    if (contour.size() < 5)
        return 0.0;

    cv::Moments m = cv::moments(contour);
    if (m.m00 == 0.0)
        return 0.0;

    // 30kinci dereceden normalized central momentler
    double mu20 = m.mu20 / m.m00;
    double mu02 = m.mu02 / m.m00;
    double mu11 = m.mu11 / m.m00;

    // d6z de1fer hesaplama (eigenvalues)
    double common = std::sqrt(4.0 * mu11 * mu11 + (mu20 - mu02) * (mu20 - mu02));
    if (common == 0.0)
        return 1.0;

    double lambda1 = (mu20 + mu02 + common) / 2.0;
    double lambda2 = (mu20 + mu02 - common) / 2.0;

    if (lambda2 == 0.0)
        return 0.0;

    return lambda1 / lambda2;
}

ShapeAnalysisResult analyzeBalloonShape(const std::vector<cv::Point>& contour)
{
    ShapeAnalysisResult result;

    // Alan ve e7evre hesapla
    double area      = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);

    // d6n eleme: e7ok kfce7fck konturlar31 reddet
    if (area < MIN_CONTOUR_AREA || perimeter == 0.0) {
        return result;  // isBalloon = false
    }

    // Geometrik metrikleri hesapla
    double circularity = calculateCircularity(contour, area, perimeter);
    double convexity   = calculateConvexity(contour, area);
    double inertia     = calculateInertiaRatio(contour);

    // Metrikleri kaydet
    result.metrics.area        = static_cast<int>(area);
    result.metrics.circularity = circularity;
    result.metrics.convexity   = convexity;
    result.metrics.inertia     = inertia;

    // Kriterleri kontrol et
    bool isCircular    = circularity > CIRCULARITY_THRESHOLD;
    bool isConvex      = convexity   > CONVEXITY_THRESHOLD;
    bool isProperRatio = (inertia > INERTIA_RATIO_MIN) && 
                        (inertia < INERTIA_RATIO_MAX);

    // Tfcm kriterler sa1flanmal31
    result.isBalloon = isCircular && isConvex && isProperRatio;

    // Gfcven skoru hesapla (0-1 aras31)
    if (result.isBalloon) {
        double circScore    = std::min(circularity, 1.0) * 0.4;
        double convScore    = std::min(convexity,   1.0) * 0.4;
        double inertiaScore = (1.0 - std::abs(1.0 - inertia)) * 0.2;
        result.confidence   = circScore + convScore + inertiaScore;
    }

    return result;
}

} // namespace sancak
