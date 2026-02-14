/**
 * @file test_distance_estimator.cpp
 * @brief DistanceEstimator birim testleri
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sancak/distance_estimator.hpp"

using namespace sancak;
using Catch::Approx;

namespace {
    DistanceConfig makeConfig() {
        DistanceConfig cfg;
        cfg.known_balloon_diameter_m = 0.12f;
        cfg.focal_length_px = 600.0f;
        cfg.min_distance_m = 3.0f;
        cfg.max_distance_m = 20.0f;
        return cfg;
    }
}

TEST_CASE("DistanceEstimator - Balon yarıçapından mesafe", "[distance]") {
    DistanceEstimator de;
    de.initialize(makeConfig());

    SECTION("Bilinen boyut doğru mesafe vermeli") {
        // D = (0.12 * 600) / (2 * radius)
        // radius=36px → D = 72/72 = 1.0 → clamp to 3.0
        // radius=12px → D = 72/24 = 3.0
        auto est = de.fromBalloonRadius(12.0f);
        REQUIRE(est.distance_m == Approx(3.0f).margin(0.1f));
    }

    SECTION("Büyük yarıçap → yakın mesafe") {
        auto est1 = de.fromBalloonRadius(30.0f);
        auto est2 = de.fromBalloonRadius(10.0f);
        REQUIRE(est1.distance_m < est2.distance_m);
    }

    SECTION("Sıfır yarıçap → geçersiz") {
        auto est = de.fromBalloonRadius(0.0f);
        REQUIRE(est.distance_m == 0.0f);
    }
}

TEST_CASE("DistanceEstimator - Bbox yüksekliğinden mesafe", "[distance]") {
    DistanceEstimator de;
    de.initialize(makeConfig());

    SECTION("Doğru hesaplama") {
        // D = (0.40 * 600) / 40 = 6.0
        auto est = de.fromBboxHeight(40.0f, 0.40f);
        REQUIRE(est.distance_m == Approx(6.0f).margin(0.1f));
    }

    SECTION("Küçük bbox → uzak mesafe") {
        // near: D = (0.40 * 600) / 60 = 4.0
        // far_: D = (0.40 * 600) / 20 = 12.0
        auto near = de.fromBboxHeight(60.0f, 0.40f);
        auto far_ = de.fromBboxHeight(20.0f, 0.40f);
        REQUIRE(far_.distance_m > near.distance_m);
    }
}

TEST_CASE("DistanceEstimator - Birleşik tahmin", "[distance]") {
    DistanceEstimator de;
    de.initialize(makeConfig());

    SECTION("Her iki kaynak → ağırlıklı ortalama") {
        auto est = de.combined(15.0f, 80.0f, 0.25f);
        REQUIRE(est.distance_m > 0.0f);
        REQUIRE(est.confidence > 0.0f);
    }
}
