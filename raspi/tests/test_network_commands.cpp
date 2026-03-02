/**
 * @file test_network_commands.cpp
 * @brief PC -> RasPi satır-tabanlı komutların (SNK2 control-plane) parsing/uygulama testleri
 */
#include <catch2/catch_test_macros.hpp>

#include "sancak/combat_pipeline.hpp"

using namespace sancak;

namespace {

SystemConfig makeTestConfig() {
    SystemConfig cfg;
    cfg.network.telemetry_enabled = true;
    cfg.autonomous = false;

    cfg.geofence.pan_min_deg = -5.0f;
    cfg.geofence.pan_max_deg = 5.0f;
    cfg.geofence.tilt_min_deg = -3.0f;
    cfg.geofence.tilt_max_deg = 3.0f;

    cfg.balloon.h_min = 5;
    cfg.balloon.h_max = 25;
    cfg.balloon.s_min = 100;
    cfg.balloon.s_max = 255;
    cfg.balloon.v_min = 100;
    cfg.balloon.v_max = 255;
    return cfg;
}

} // namespace

TEST_CASE("NetworkCommands - OVERLAY ON/OFF", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());

    REQUIRE(p._testOverlayEnabled() == true);

    p._testHandleNetworkCommand("<OVERLAY:OFF>");
    REQUIRE(p._testOverlayEnabled() == false);

    p._testHandleNetworkCommand("<OVERLAY:ON>");
    REQUIRE(p._testOverlayEnabled() == true);
}

TEST_CASE("NetworkCommands - DETECT START/STOP", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());

    REQUIRE(p._testDetectionEnabled() == true);

    p._testHandleNetworkCommand("<DETECT:STOP>");
    REQUIRE(p._testDetectionEnabled() == false);

    p._testHandleNetworkCommand("<DETECT:START>");
    REQUIRE(p._testDetectionEnabled() == true);
}

TEST_CASE("NetworkCommands - MODE FULL_AUTO/MANUAL", "[netcmd]") {
    CombatPipeline p;
    auto cfg = makeTestConfig();
    cfg.autonomous = false;
    p._testSetConfig(cfg);

    REQUIRE(p._testConfig().autonomous == false);

    p._testHandleNetworkCommand("<MODE:FULL_AUTO>");
    REQUIRE(p._testConfig().autonomous == true);
    REQUIRE(p._testDetectionEnabled() == true);

    p._testHandleNetworkCommand("<MODE:MANUAL>");
    REQUIRE(p._testConfig().autonomous == false);
}

TEST_CASE("NetworkCommands - MOVE clamps to geofence", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());
    p._testSetPoseDeg(0.0f, 0.0f);

    p._testHandleNetworkCommand("<MOVE:LEFT>");
    REQUIRE(p._testPanDeg() == Approx(-1.0f));

    p._testHandleNetworkCommand("<MOVE:RIGHT>");
    REQUIRE(p._testPanDeg() == Approx(0.0f));

    p._testHandleNetworkCommand("<MOVE:FORWARD>");
    REQUIRE(p._testTiltDeg() == Approx(1.0f));

    p._testHandleNetworkCommand("<MOVE:BACK>");
    REQUIRE(p._testTiltDeg() == Approx(0.0f));

    // clamp
    p._testSetPoseDeg(5.0f, 3.0f);
    p._testHandleNetworkCommand("<MOVE:RIGHT>");
    REQUIRE(p._testPanDeg() == Approx(5.0f));
    p._testHandleNetworkCommand("<MOVE:FORWARD>");
    REQUIRE(p._testTiltDeg() == Approx(3.0f));
}

TEST_CASE("NetworkCommands - GEOFENCE updates config and clamps pose", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());
    p._testSetPoseDeg(4.0f, 2.0f);

    p._testHandleNetworkCommand("<GEOFENCE:-2,2,-1,1>");

    REQUIRE(p._testConfig().geofence.pan_min_deg == Approx(-2.0f));
    REQUIRE(p._testConfig().geofence.pan_max_deg == Approx(2.0f));
    REQUIRE(p._testConfig().geofence.tilt_min_deg == Approx(-1.0f));
    REQUIRE(p._testConfig().geofence.tilt_max_deg == Approx(1.0f));

    REQUIRE(p._testPanDeg() == Approx(2.0f));
    REQUIRE(p._testTiltDeg() == Approx(1.0f));
}

TEST_CASE("NetworkCommands - SET updates balloon HSV with clamp", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());

    p._testHandleNetworkCommand("<SET:H_MIN,10>");
    REQUIRE(p._testConfig().balloon.h_min == 10);

    p._testHandleNetworkCommand("<SET:H_MAX,200>");
    // H üst sınırı OpenCV HSV H kanalına göre 179'la clamp
    REQUIRE(p._testConfig().balloon.h_max == 179);

    p._testHandleNetworkCommand("<SET:S_MIN,-5>");
    REQUIRE(p._testConfig().balloon.s_min == 0);

    p._testHandleNetworkCommand("<SET:V_MAX,300>");
    REQUIRE(p._testConfig().balloon.v_max == 255);
}

TEST_CASE("NetworkCommands - FP16 ON/OFF updates model path", "[netcmd]") {
    CombatPipeline p;
    p._testSetConfig(makeTestConfig());

    // default config path: models/best.onnx -> models/best_fp16.onnx
    REQUIRE(p._testFp16Enabled() == false);

    p._testHandleNetworkCommand("<FP16:ON>");
    REQUIRE(p._testFp16Enabled() == true);
    REQUIRE(p._testYoloModelPath().find("_fp16.onnx") != std::string::npos);

    p._testHandleNetworkCommand("<FP16:OFF>");
    REQUIRE(p._testFp16Enabled() == false);
    REQUIRE(p._testYoloModelPath().find("_fp16.onnx") == std::string::npos);
}
