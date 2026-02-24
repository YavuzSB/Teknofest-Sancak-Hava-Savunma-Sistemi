/**
 * @file target_tracker.hpp
 * @brief Sancak Hava Savunma Sistemi - Çoklu Hedef Takip Sistemi
 *
 * IoU tabanlı eşleştirme ile frame'ler arası hedef takibi.
 * Her hedefe benzersiz ID atar ve hız vektörünü hesaplar.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <vector>
#include <unordered_map>

namespace sancak {

/**
 * @class TargetTracker
 * @brief IoU tabanlı çoklu hedef takip sistemi
 *
 * Algoritma:
 *  1. Yeni tespitleri mevcut track'lerle IoU ile eşleştir
 *  2. Eşleşen track'leri güncelle
 *  3. Eşleşmeyen tespitler → yeni track oluştur
 *  4. Eşleşmeyen track'ler → lost_frames artır
 *  5. max_lost_frames geçenleri sil
 *  6. Her track için hız vektörü hesapla (EMA)
 */
class TargetTracker {
public:
    TargetTracker() = default;

    /**
     * @brief Konfigürasyon ile başlat
     */
    void initialize(const TrackingConfig& config);

    /**
     * @brief Yeni frame tespitlerini işle ve track'leri güncelle
     * @param detections Güncel frame tespitleri
     * @return Aktif track'lerin listesi
     */
    std::vector<TrackedTarget>& update(const std::vector<Detection>& detections);

    /**
     * @brief Tüm track'leri sıfırla
     */
    void reset();

    /**
     * @brief Aktif track sayısı
     */
    size_t activeCount() const;

    /**
     * @brief En öncelikli düşman hedefini döndür
     */
    const TrackedTarget* getPriorityTarget() const;

private:
    /// İki bounding box arası IoU hesapla
    static float computeIoU(const cv::Rect2f& a, const cv::Rect2f& b);

    /// Bounding box merkezi
    static cv::Point2f getCenter(const cv::Rect2f& r);

    /// İki bbox merkezi arası Öklid mesafesi (px)
    static float centerDistance(const cv::Rect2f& a, const cv::Rect2f& b);

    /// Greedy IoU eşleştirme
    std::vector<std::pair<int, int>> matchDetections(
        const std::vector<Detection>& detections) const;

    /// Yeni track ID üret
    int nextTrackId();

    /// Öncelik skoru hesapla
    static float priorityScore(const TrackedTarget& t);

    TrackingConfig config_;
    std::vector<TrackedTarget> tracks_;
    int next_id_ = 0;
};

} // namespace sancak
