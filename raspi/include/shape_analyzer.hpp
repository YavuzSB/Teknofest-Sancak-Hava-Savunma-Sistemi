/**
 * @file shape_analyzer.hpp
 * @brief Sekil Analizi Modulu - Balon tespiti icin geometrik filtreler
 *
 * Dairesellik, disbukeylik ve atalet orani hesaplamalari ile
 * konturun balon olup olmadigini belirler.
 */
#pragma once

#include "common_defs.hpp"
#include <opencv2/opencv.hpp>

namespace sancak {

// Sekil analiz esikleri
constexpr double CIRCULARITY_THRESHOLD = 0.6;
constexpr double CONVEXITY_THRESHOLD   = 0.85;
constexpr double INERTIA_RATIO_MIN      = 0.3;
constexpr double INERTIA_RATIO_MAX      = 1.5;
constexpr int    MIN_CONTOUR_AREA      = 500;

/// Sekil analizi debug bilgisi
struct ShapeMetrics {
    int    area         = 0;
    double circularity  = 0.0;
    double convexity    = 0.0;
    double inertia      = 0.0;
};

/// Balon sekil analizi sonucu
struct ShapeAnalysisResult {
    bool    isBalloon  = false;
    double  confidence = 0.0;
    ShapeMetrics metrics;
};

/**
 * @brief Dairesellik (Circularity) hesaplar.
 * Formul: 4*pi*A / P^2
 */
double calculateCircularity(const std::vector<cv::Point>& contour,
                            double area, 
                            double perimeter);

/**
 * @brief Disbukeylik (Convexity) hesaplar.
 * Formul: A / ConvexHullArea
 */
double calculateConvexity(const std::vector<cv::Point>& contour, 
                         double area);

/**
 * @brief Atalet Orani (Inertia Ratio) hesaplar.
 */
double calculateInertiaRatio(const std::vector<cv::Point>& contour);

/**
 * @brief Konturun balon sekline uyup uymadigini analiz eder.
 */
ShapeAnalysisResult analyzeBalloonShape(const std::vector<cv::Point>& contour);

} // namespace sancak