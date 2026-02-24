/**
 * @file test_trigger_controller.cpp
 * @brief TriggerController birim testleri
 */
#include <catch2/catch_test_macros.hpp>

#include "sancak/trigger_controller.hpp"

using namespace sancak;

TEST_CASE("TriggerController - kilitlenme ve burst/cooldown", "[trigger]") {
    TriggerController tc;
    tc.initialize(
        15.0f, // aim_tolerance_px
        3,     // lock_frames_required
        0,     // burst_duration_ms (test için 0)
        0      // cooldown_ms (test için 0)
    );

    const cv::Point2f crosshair(100.0f, 100.0f);
    const cv::Point2f target_ok(110.0f, 100.0f); // 10px
    const cv::Point2f target_bad(200.0f, 200.0f);

    // Kilit sayacı dışarı çıkınca sıfırlanmalı
    REQUIRE_FALSE(tc.update(target_ok, crosshair));
    REQUIRE_FALSE(tc.update(target_bad, crosshair));

    // 3 ardışık frame içinde olunca ateşlemeli
    REQUIRE_FALSE(tc.update(target_ok, crosshair));
    REQUIRE_FALSE(tc.update(target_ok, crosshair));
    REQUIRE(tc.update(target_ok, crosshair)); // FIRING başlar

    // burst=0 olduğundan bir sonraki update'te COOLDOWN'a düşer ve ateş kapalı olur
    REQUIRE_FALSE(tc.update(target_ok, crosshair));

    // cooldown=0 olduğundan tekrar LOCKING'e geçer ama yeniden 3 frame kilit ister
    REQUIRE_FALSE(tc.update(target_ok, crosshair));
    REQUIRE_FALSE(tc.update(target_ok, crosshair));
    REQUIRE(tc.update(target_ok, crosshair));
}
