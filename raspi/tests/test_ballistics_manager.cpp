/**
 * @file test_ballistics_manager.cpp
 * @brief BallisticsManager birim testleri
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sancak/ballistics_manager.hpp"

using namespace sancak;
using Catch::Approx;

namespace {
    BallisticsConfig makeDefaultConfig() {
        BallisticsConfig cfg;
        cfg.camera_barrel_offset_x_m = 0.0f;
        cfg.camera_barrel_offset_y_m = 0.05f;
        cfg.muzzle_velocity_mps = 90.0f;
        cfg.zeroing_distance_m = 10.0f;
        cfg.manual_offset_x_px = 0.0f;
        cfg.manual_offset_y_px = 0.0f;
        cfg.lookup_table = {
            {  5.0f, 15.0f, 0.0f },
            { 10.0f,  6.0f, 0.0f },
            { 15.0f,  1.0f, 0.0f }
        };
        return cfg;
    }
}

TEST_CASE("BallisticsManager - Paralaks hesaplaması", "[ballistics]") {
    BallisticsManager bm;
    bm.initialize(makeDefaultConfig());

    SECTION("Sıfırlama mesafesinde paralaks ~0 olmalı") {
        float parallax = bm.calculateParallax(10.0f);
        REQUIRE(std::abs(parallax) < 0.001f);
    }

    SECTION("Yakın mesafede paralaks pozitif olmalı") {
        float parallax = bm.calculateParallax(5.0f);
        REQUIRE(parallax > 0.0f);
    }

    SECTION("Uzak mesafede paralaks negatif olmalı") {
        float parallax = bm.calculateParallax(15.0f);
        REQUIRE(parallax < 0.0f);
    }
}

TEST_CASE("BallisticsManager - Balistik düşüş", "[ballistics]") {
    BallisticsManager bm;
    bm.initialize(makeDefaultConfig());

    SECTION("Düşüş mesafeyle artmalı") {
        float drop5  = bm.calculateDrop(5.0f);
        float drop10 = bm.calculateDrop(10.0f);
        float drop15 = bm.calculateDrop(15.0f);

        REQUIRE(drop5 > 0.0f);
        REQUIRE(drop10 > drop5);
        REQUIRE(drop15 > drop10);
    }

    SECTION("10m mesafede düşüş hesabı doğru olmalı") {
        // t = 10/90 ≈ 0.111s, drop = 0.5 * 9.81 * 0.111^2 ≈ 0.0605m
        float drop = bm.calculateDrop(10.0f);
        REQUIRE(drop == Approx(0.0605f).margin(0.005f));
    }
}

TEST_CASE("BallisticsManager - Lookup table interpolasyonu", "[ballistics]") {
    BallisticsManager bm;
    bm.initialize(makeDefaultConfig());

    float dx, dy;

    SECTION("Tam değer: 10m") {
        bm.interpolateFromTable(10.0f, dx, dy);
        REQUIRE(dy == Approx(6.0f));
    }

    SECTION("Ara değer: 7.5m → interpolasyon") {
        bm.interpolateFromTable(7.5f, dx, dy);
        // 5m=15, 10m=6 arası → 7.5m = 15 + 0.5*(6-15) = 10.5
        REQUIRE(dy == Approx(10.5f));
    }

    SECTION("Sınır altı: 3m → ilk değer") {
        bm.interpolateFromTable(3.0f, dx, dy);
        REQUIRE(dy == Approx(15.0f));
    }

    SECTION("Sınır üstü: 20m → son değer") {
        bm.interpolateFromTable(20.0f, dx, dy);
        REQUIRE(dy == Approx(1.0f));
    }
}

TEST_CASE("BallisticsManager - Manuel offset", "[ballistics]") {
    BallisticsManager bm;
    auto cfg = makeDefaultConfig();
    cfg.manual_offset_x_px = 5.0f;
    cfg.manual_offset_y_px = -3.0f;
    bm.initialize(cfg);

    auto corr = bm.calculate(10.0f);
    REQUIRE(corr.dx_px == Approx(5.0f).margin(0.5f));
    // dy = table(6.0) + manual(-3.0) = 3.0
    REQUIRE(corr.dy_px == Approx(3.0f).margin(0.5f));
}

TEST_CASE("BallisticsManager - applyCorrection", "[ballistics]") {
    BallisticsManager bm;
    auto cfg = makeDefaultConfig();
    cfg.manual_offset_x_px = 2.0f;
    cfg.manual_offset_y_px = -1.0f;
    bm.initialize(cfg);

    // 7.5m: 5m(15) ile 10m(6) arası -> 10.5, sonra manuel -1 -> 9.5
    const cv::Point2f raw(100.0f, 200.0f);
    const cv::Point2f corr = bm.applyCorrection(raw, 7.5f);

    REQUIRE(corr.x == Approx(102.0f).margin(0.001f));
    REQUIRE(corr.y == Approx(209.5f).margin(0.001f));
}
