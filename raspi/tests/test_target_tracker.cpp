/**
 * @file test_target_tracker.cpp
 * @brief TargetTracker birim testleri
 */
#include <catch2/catch_test_macros.hpp>
#include "sancak/target_tracker.hpp"

using namespace sancak;

namespace {
    TrackingConfig makeConfig() {
        TrackingConfig cfg;
        cfg.iou_threshold = 0.3f;
        cfg.max_lost_frames = 5;
        cfg.min_confirm_frames = 2;
        cfg.velocity_smoothing = 0.7f;
        return cfg;
    }

    Detection makeDet(float x, float y, float w, float h,
                      TargetClass cls = TargetClass::kDrone, float conf = 0.9f) {
        Detection d;
        d.bbox = cv::Rect2f(x, y, w, h);
        d.confidence = conf;
        d.target_class = cls;
        d.class_id = static_cast<int>(cls);
        return d;
    }
}

TEST_CASE("TargetTracker - Yeni hedef oluşturma", "[tracker]") {
    TargetTracker tracker;
    tracker.initialize(makeConfig());

    std::vector<Detection> dets = { makeDet(100, 100, 50, 50) };
    auto& tracks = tracker.update(dets);

    REQUIRE(tracks.size() == 1);
    REQUIRE(tracks[0].track_id >= 0);
    REQUIRE(tracks[0].age == 1);
}

TEST_CASE("TargetTracker - Hedef eşleşmesi (IoU)", "[tracker]") {
    TargetTracker tracker;
    tracker.initialize(makeConfig());

    // İlk frame
    tracker.update({ makeDet(100, 100, 50, 50) });

    // İkinci frame (biraz kaymış)
    auto& tracks = tracker.update({ makeDet(105, 105, 50, 50) });

    REQUIRE(tracks.size() == 1);
    REQUIRE(tracks[0].age == 2);  // Aynı track devam etmeli
}

TEST_CASE("TargetTracker - Merkez mesafesi fallback (IoU düşerse)", "[tracker]") {
    TargetTracker tracker;
    auto cfg = makeConfig();
    cfg.iou_threshold = 0.3f;
    cfg.max_center_distance_px = 50.0f;
    tracker.initialize(cfg);

    // Frame 1: küçük bbox
    auto& t1 = tracker.update({ makeDet(100, 100, 50, 50) });
    REQUIRE(t1.size() == 1);
    const int id0 = t1[0].track_id;

    // Frame 2: bbox boyutu aniden büyüsün ama merkez aynı kalsın.
    // (100,100,50,50) merkezi (125,125). Yeni bbox: (75,75,100,100) merkezi yine (125,125).
    // IoU = 0.25 (<0.3) olduğu için IoU eşleşmesi başarısız; fallback merkez mesafesi ile eşleşmeli.
    auto& t2 = tracker.update({ makeDet(75, 75, 100, 100) });

    REQUIRE(t2.size() == 1);
    REQUIRE(t2[0].track_id == id0);
    REQUIRE(t2[0].age == 2);
}

TEST_CASE("TargetTracker - Hedef kaybı", "[tracker]") {
    TargetTracker tracker;
    auto cfg = makeConfig();
    cfg.max_lost_frames = 3;
    tracker.initialize(cfg);

    // İlk frame
    tracker.update({ makeDet(100, 100, 50, 50) });

    // Sonraki frame'lerde tespit yok
    for (int i = 0; i < 4; ++i) {
        tracker.update({});
    }

    REQUIRE(tracker.activeCount() == 0);
}

TEST_CASE("TargetTracker - Çoklu hedef", "[tracker]") {
    TargetTracker tracker;
    tracker.initialize(makeConfig());

    std::vector<Detection> dets = {
        makeDet(100, 100, 50, 50, TargetClass::kDrone),
        makeDet(300, 200, 60, 60, TargetClass::kJet),
        makeDet(500, 100, 40, 40, TargetClass::kFriendly)
    };

    auto tracks = tracker.update(dets);
    REQUIRE(tracks.size() == 3);
}

TEST_CASE("TargetTracker - Öncelik: düşman > dost", "[tracker]") {
    TargetTracker tracker;
    auto cfg = makeConfig();
    cfg.min_confirm_frames = 1;  // Hızlı onay
    tracker.initialize(cfg);

    std::vector<Detection> dets = {
        makeDet(100, 100, 80, 80, TargetClass::kDrone, 0.95f),
        makeDet(300, 200, 60, 60, TargetClass::kFriendly, 0.99f)
    };

    tracker.update(dets);

    const auto* priority = tracker.getPriorityTarget();
    REQUIRE(priority != nullptr);
    REQUIRE(IsEnemy(priority->detection.target_class));
}

TEST_CASE("TargetTracker - Hız hesaplama", "[tracker]") {
    TargetTracker tracker;
    tracker.initialize(makeConfig());

    // Frame 1: (100, 100)
    tracker.update({ makeDet(100, 100, 50, 50) });
    // Frame 2: (110, 100) → 10px sağa
    auto& tracks = tracker.update({ makeDet(110, 100, 50, 50) });

    REQUIRE(tracks.size() == 1);
    REQUIRE(tracks[0].velocity.x > 0.0f);  // Sağa hareket
}

TEST_CASE("TargetTracker - Reset", "[tracker]") {
    TargetTracker tracker;
    tracker.initialize(makeConfig());

    tracker.update({ makeDet(100, 100, 50, 50) });
    REQUIRE(tracker.activeCount() == 1);

    tracker.reset();
    REQUIRE(tracker.activeCount() == 0);
}
