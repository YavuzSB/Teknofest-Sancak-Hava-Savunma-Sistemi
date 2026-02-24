/**
 * @file target_tracker.cpp
 * @brief Çoklu Hedef Takip Sistemi - Implementasyon
 *
 * IoU tabanlı greedy eşleştirme, EMA hız hesaplama,
 * otomatik ID atama ve öncelik sıralama.
 */
#include "sancak/target_tracker.hpp"
#include "sancak/logger.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sancak {

namespace {
constexpr std::size_t kAffiliationHistoryMax = 5;

Affiliation majorityAffiliation(const std::deque<Affiliation>& hist, Affiliation fallback) {
    int friend_count = 0;
    int foe_count = 0;
    int unknown_count = 0;
    for (Affiliation a : hist) {
        switch (a) {
            case Affiliation::Friend:  friend_count++; break;
            case Affiliation::Foe:     foe_count++; break;
            default:                   unknown_count++; break;
        }
    }

    // Mode seçimi; eşitlikte önceki değeri koru (hysteresis)
    if (friend_count > foe_count && friend_count > unknown_count) {
        return Affiliation::Friend;
    }
    if (foe_count > friend_count && foe_count > unknown_count) {
        return Affiliation::Foe;
    }
    if (unknown_count > friend_count && unknown_count > foe_count) {
        return Affiliation::Unknown;
    }
    return fallback;
}
}

void TargetTracker::initialize(const TrackingConfig& config) {
    config_ = config;
    tracks_.clear();
    next_id_ = 0;
    SANCAK_LOG_INFO("Target Tracker başlatıldı | IoU eşik: {:.2f} | Merkez mesafe: {:.1f}px | Maks kayıp: {}",
                    config_.iou_threshold, config_.max_center_distance_px, config_.max_lost_frames);
}

float TargetTracker::computeIoU(const cv::Rect2f& a, const cv::Rect2f& b) {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width,  b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);

    float inter_w = std::max(0.0f, x2 - x1);
    float inter_h = std::max(0.0f, y2 - y1);
    float inter_area = inter_w * inter_h;

    float area_a = a.width * a.height;
    float area_b = b.width * b.height;
    float union_area = area_a + area_b - inter_area;

    if (union_area <= 0.0f) return 0.0f;
    return inter_area / union_area;
}

cv::Point2f TargetTracker::getCenter(const cv::Rect2f& r) {
    return cv::Point2f(r.x + r.width / 2.0f, r.y + r.height / 2.0f);
}

float TargetTracker::centerDistance(const cv::Rect2f& a, const cv::Rect2f& b) {
    const cv::Point2f ca = getCenter(a);
    const cv::Point2f cb = getCenter(b);
    const float dx = ca.x - cb.x;
    const float dy = ca.y - cb.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<std::pair<int, int>> TargetTracker::matchDetections(
    const std::vector<Detection>& detections) const {

    std::vector<std::pair<int, int>> matches;  // (track_idx, det_idx)
    std::vector<bool> track_matched(tracks_.size(), false);
    std::vector<bool> det_matched(detections.size(), false);

    // IoU matrisini hesapla ve greedy eşleştir
    struct Match {
        int track_idx;
        int det_idx;
        float iou;
    };
    std::vector<Match> candidates;

    for (size_t t = 0; t < tracks_.size(); ++t) {
        for (size_t d = 0; d < detections.size(); ++d) {
            float iou = computeIoU(tracks_[t].detection.bbox, detections[d].bbox);
            if (iou >= config_.iou_threshold) {
                candidates.push_back({static_cast<int>(t), static_cast<int>(d), iou});
            }
        }
    }

    // IoU'ya göre azalan sırala
    std::sort(candidates.begin(), candidates.end(),
              [](const Match& a, const Match& b) { return a.iou > b.iou; });

    // Greedy eşleştirme
    for (const auto& c : candidates) {
        if (!track_matched[c.track_idx] && !det_matched[c.det_idx]) {
            matches.emplace_back(c.track_idx, c.det_idx);
            track_matched[c.track_idx] = true;
            det_matched[c.det_idx]     = true;
        }
    }

    return matches;
}

std::vector<TrackedTarget>& TargetTracker::update(const std::vector<Detection>& detections) {
    auto matches = matchDetections(detections);

    // Eşleşen/eşleşmeyen indeksleri bul
    std::vector<bool> track_matched(tracks_.size(), false);
    std::vector<bool> det_matched(detections.size(), false);

    for (const auto& [t_idx, d_idx] : matches) {
        track_matched[t_idx] = true;
        det_matched[d_idx]   = true;

        auto& track = tracks_[t_idx];

        const Affiliation prev_aff = track.detection.affiliation;

        // Önceki merkez
        const cv::Point2f prev_center = getCenter(track.detection.bbox);

        // Yeni merkez
        const auto& det = detections[d_idx];
        const cv::Point2f new_center = getCenter(det.bbox);

        // Hız hesapla (EMA)
        cv::Point2f raw_vel = new_center - prev_center;
        float alpha = config_.velocity_smoothing;
        track.velocity.x = alpha * track.velocity.x + (1.0f - alpha) * raw_vel.x;
        track.velocity.y = alpha * track.velocity.y + (1.0f - alpha) * raw_vel.y;

        // IFF Majority Voting: ham IFF sonucunu buffer'a ekle, kararlı kimliği ata
        track.affiliation_history.push_back(det.affiliation);
        while (track.affiliation_history.size() > kAffiliationHistoryMax) {
            track.affiliation_history.pop_front();
        }

        // Tespiti güncelle
        track.detection   = det;
        track.detection.affiliation = majorityAffiliation(track.affiliation_history, prev_aff);
        track.age++;
        track.lost_frames = 0;
    }

    // Aşama 2: IoU ile eşleşmeyenler için merkez mesafesi fallback
    // Not: Boyut/şekil ani değişince IoU düşebilir; merkez yakınlığıyla track devam ettirilir.
    {
        std::vector<int> unmatched_tracks;
        unmatched_tracks.reserve(tracks_.size());
        for (size_t t = 0; t < tracks_.size(); ++t) {
            if (!track_matched[t]) unmatched_tracks.push_back(static_cast<int>(t));
        }

        std::vector<int> unmatched_dets;
        unmatched_dets.reserve(detections.size());
        for (size_t d = 0; d < detections.size(); ++d) {
            if (!det_matched[d]) unmatched_dets.push_back(static_cast<int>(d));
        }

        struct DistCand {
            int track_idx;
            int det_idx;
            float dist;
        };
        std::vector<DistCand> dist_candidates;

        for (int t_idx : unmatched_tracks) {
            for (int d_idx : unmatched_dets) {
                const float dist = centerDistance(tracks_[t_idx].detection.bbox, detections[d_idx].bbox);
                if (dist <= config_.max_center_distance_px) {
                    dist_candidates.push_back({t_idx, d_idx, dist});
                }
            }
        }

        std::sort(dist_candidates.begin(), dist_candidates.end(),
                  [](const DistCand& a, const DistCand& b) { return a.dist < b.dist; });

        for (const auto& c : dist_candidates) {
            if (track_matched[c.track_idx] || det_matched[c.det_idx]) continue;

            track_matched[c.track_idx] = true;
            det_matched[c.det_idx]     = true;

            auto& track = tracks_[c.track_idx];

            const Affiliation prev_aff = track.detection.affiliation;

            const cv::Point2f prev_center = getCenter(track.detection.bbox);
            const auto& det = detections[c.det_idx];
            const cv::Point2f new_center = getCenter(det.bbox);

            // Hız hesapla (EMA)
            cv::Point2f raw_vel = new_center - prev_center;
            float alpha = config_.velocity_smoothing;
            track.velocity.x = alpha * track.velocity.x + (1.0f - alpha) * raw_vel.x;
            track.velocity.y = alpha * track.velocity.y + (1.0f - alpha) * raw_vel.y;

            track.affiliation_history.push_back(det.affiliation);
            while (track.affiliation_history.size() > kAffiliationHistoryMax) {
                track.affiliation_history.pop_front();
            }

            track.detection   = det;
            track.detection.affiliation = majorityAffiliation(track.affiliation_history, prev_aff);
            track.age++;
            track.lost_frames = 0;
        }
    }

    // Eşleşmeyen track'ler → kayıp sayacını artır
    for (size_t t = 0; t < tracks_.size(); ++t) {
        if (!track_matched[t]) {
            tracks_[t].lost_frames++;
        }
    }

    // Eşleşmeyen tespitler → yeni track oluştur
    for (size_t d = 0; d < detections.size(); ++d) {
        if (!det_matched[d]) {
            TrackedTarget nt;
            nt.track_id    = nextTrackId();
            nt.detection   = detections[d];
            nt.affiliation_history.clear();
            nt.affiliation_history.push_back(detections[d].affiliation);
            nt.velocity    = {0.0f, 0.0f};
            nt.age         = 1;
            nt.lost_frames = 0;
            tracks_.push_back(nt);

            SANCAK_LOG_DEBUG("Yeni hedef: ID={} | Sınıf={} | Güven={:.2f}",
                             nt.track_id,
                             TargetClassName(nt.detection.target_class),
                             nt.detection.confidence);
        }
    }

    // Ölü track'leri temizle
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
                       [this](const TrackedTarget& t) {
                           return t.lost_frames > config_.max_lost_frames;
                       }),
        tracks_.end()
    );

    // Öncelik hesapla
    for (auto& t : tracks_) {
        t.is_priority = (IsEnemy(t.detection.target_class) &&
                         t.age >= config_.min_confirm_frames &&
                         t.lost_frames == 0);
    }

    return tracks_;
}

void TargetTracker::reset() {
    tracks_.clear();
    next_id_ = 0;
    SANCAK_LOG_INFO("Target Tracker sıfırlandı");
}

size_t TargetTracker::activeCount() const {
    return std::count_if(tracks_.begin(), tracks_.end(),
                         [](const TrackedTarget& t) { return t.lost_frames == 0; });
}

const TrackedTarget* TargetTracker::getPriorityTarget() const {
    const TrackedTarget* best = nullptr;
    float best_score = -1.0f;

    for (const auto& t : tracks_) {
        if (!t.is_priority) continue;
        float score = priorityScore(t);
        if (score > best_score) {
            best_score = score;
            best = &t;
        }
    }

    return best;
}

int TargetTracker::nextTrackId() {
    return next_id_++;
}

float TargetTracker::priorityScore(const TrackedTarget& t) {
    // Öncelik: yakınlık (büyük bbox) + güven + balon bulunmuşsa bonus
    float size_score = t.detection.bbox.width * t.detection.bbox.height / 10000.0f;
    float conf_score = t.detection.confidence;
    float balloon_bonus = t.balloon.found ? 2.0f : 0.0f;
    float age_bonus = std::min(static_cast<float>(t.age) / 10.0f, 1.0f);

    return size_score + conf_score + balloon_bonus + age_bonus;
}

} // namespace sancak
