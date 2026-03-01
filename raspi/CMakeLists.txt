# ============================================================================
# SANCAK HAVA SAVUNMA SİSTEMİ - CMake Build Sistemi
# ============================================================================
#
# Yeni profesyonel yapı (sancak/ namespace):
#   sancak_combat  → Ana çalıştırılabilir (YOLO + Balon + Balistik pipeline)
#
# Eski modüler yapı (geriye uyumluluk için korunmuştur):
#   main_detector  → Eski balon tespit sistemi
#
# Derleme (Linux/Raspberry Pi):
#   mkdir -p build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j$(nproc)
#
# Derleme (Cross-compile Pi 5 için):
#   cmake .. -DCMAKE_BUILD_TYPE=Release -DSANCAK_ENABLE_NATIVE_ARCH=ON
#
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(SancakVision
    VERSION 2.0.0
    LANGUAGES CXX
    DESCRIPTION "Sancak Hava Savunma Sistemi - Teknofest"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# SEÇENEKLER
# ============================================================================
option(SANCAK_BUILD_COMBAT     "Build new combat pipeline (YOLO + Ballistics)" ON)
option(SANCAK_BUILD_LEGACY     "Build legacy detection modules"               OFF)
option(SANCAK_ENABLE_TESTS     "Build unit tests"                             ON)
option(SANCAK_USE_ONNXRUNTIME "Use ONNX Runtime for YOLO inference"          ON)
option(SANCAK_ENABLE_CLANG_TIDY "Enable clang-tidy"                           OFF)
option(SANCAK_CLANG_TIDY_WARNINGS_AS_ERRORS "clang-tidy warnings as errors"   OFF)
option(SANCAK_ENABLE_WARNINGS  "Enable compiler warnings"                     ON)
option(SANCAK_WARNINGS_AS_ERRORS "Treat warnings as errors"                   OFF)
option(SANCAK_ENABLE_NATIVE_ARCH "Enable -march=native (GCC/Clang)"          OFF)

# Default build type
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ============================================================================
# BAĞIMLILIKLAR
# ============================================================================

# --- OpenCV (DNN modülü dahil) ---
# OpenCV keşfini daha dayanıklı hale getir
set(_opencv_candidate_dirs "")

if(DEFINED OpenCV_DIR AND OpenCV_DIR)
    list(APPEND _opencv_candidate_dirs "${OpenCV_DIR}")
endif()

if(DEFINED ENV{OpenCV_DIR} AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
    list(APPEND _opencv_candidate_dirs "$ENV{OpenCV_DIR}")
endif()

if(DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
    string(REPLACE ";" ";" _env_prefix_paths "$ENV{CMAKE_PREFIX_PATH}")
    list(APPEND _opencv_candidate_dirs ${_env_prefix_paths})
endif()

if(WIN32)
    list(APPEND _opencv_candidate_dirs
        "C:/opencv/build"
        "C:/opencv"
        "C:/tools/opencv/build"
        "C:/vcpkg/installed/x64-windows/share/opencv4"
        "C:/vcpkg/installed/x64-windows"
    )
endif()

set(_opencv_config_found FALSE)
foreach(_candidate IN LISTS _opencv_candidate_dirs)
    if(EXISTS "${_candidate}/OpenCVConfig.cmake")
        set(OpenCV_DIR "${_candidate}" CACHE PATH "Path to OpenCVConfig.cmake" FORCE)
        set(_opencv_config_found TRUE)
        break()
    endif()
    if(EXISTS "${_candidate}/opencv4/OpenCVConfig.cmake")
        set(OpenCV_DIR "${_candidate}/opencv4" CACHE PATH "Path to OpenCVConfig.cmake" FORCE)
        set(_opencv_config_found TRUE)
        break()
    endif()
endforeach()

find_package(OpenCV QUIET COMPONENTS core imgproc highgui videoio dnn)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR
        "OpenCV bulunamadı.\n"
        "Kontrol edilen aday dizinler: ${_opencv_candidate_dirs}\n"
        "Lütfen OpenCV_DIR'i OpenCVConfig.cmake içeren klasöre ayarlayın\n"
        "veya CMAKE_PREFIX_PATH'e OpenCV kurulum kökünü ekleyin."
    )
endif()

# INTERFACE target for clean linking
add_library(sancak_opencv INTERFACE)
target_include_directories(sancak_opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(sancak_opencv INTERFACE ${OpenCV_LIBS})

# ==========================================================================
# ONNX Runtime (opsiyonel ama varsayılan ON)
# ==========================================================================
if(SANCAK_USE_ONNXRUNTIME)
    set(ONNXRUNTIME_DIR "" CACHE PATH "Path to ONNX Runtime install root (contains include/ and lib/)")

    # Headers
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        HINTS ${ONNXRUNTIME_DIR} $ENV{ONNXRUNTIME_DIR}
        PATH_SUFFIXES include
    )

    # Library
    find_library(ONNXRUNTIME_LIBRARY
        NAMES onnxruntime onnxruntime_static
        HINTS ${ONNXRUNTIME_DIR} $ENV{ONNXRUNTIME_DIR}
        PATH_SUFFIXES lib lib64
    )

    if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
        message(FATAL_ERROR
            "SANCAK_USE_ONNXRUNTIME=ON ama ONNX Runtime bulunamadı.\n"
            "- ONNXRUNTIME_DIR (CMake cache) veya ENV{ONNXRUNTIME_DIR} ayarlayın (include/ ve lib/ içermeli).\n"
            "- Veya SANCAK_USE_ONNXRUNTIME=OFF yapın (OpenCV DNN fallback).\n"
        )
    endif()

    add_library(sancak_onnxruntime INTERFACE)
    target_include_directories(sancak_onnxruntime INTERFACE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(sancak_onnxruntime INTERFACE ${ONNXRUNTIME_LIBRARY})
    target_compile_definitions(sancak_onnxruntime INTERFACE SANCAK_USE_ONNXRUNTIME=1)
endif()

# ============================================================================
# ORTAK BUILD AYARLARI
# ============================================================================
add_library(sancak_build_options INTERFACE)
target_compile_features(sancak_build_options INTERFACE cxx_std_17)
target_include_directories(sancak_build_options INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SANCAK_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(sancak_build_options INTERFACE /W4 /permissive-)
        if(SANCAK_WARNINGS_AS_ERRORS)
            target_compile_options(sancak_build_options INTERFACE /WX)
        endif()
    else()
        target_compile_options(sancak_build_options INTERFACE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        )
        if(SANCAK_WARNINGS_AS_ERRORS)
            target_compile_options(sancak_build_options INTERFACE -Werror)
        endif()
        if(SANCAK_ENABLE_NATIVE_ARCH)
            target_compile_options(sancak_build_options INTERFACE -march=native)
        endif()
    endif()
endif()

# Clang-tidy
if(SANCAK_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "SANCAK_ENABLE_CLANG_TIDY=ON ama clang-tidy bulunamadı")
    endif()
    set(SANCAK_CLANG_TIDY_CMD ${CLANG_TIDY_EXE} --quiet)
    if(SANCAK_CLANG_TIDY_WARNINGS_AS_ERRORS)
        list(APPEND SANCAK_CLANG_TIDY_CMD --warnings-as-errors=*)
    endif()
    function(sancak_enable_clang_tidy target_name)
        if(TARGET ${target_name})
            set_target_properties(${target_name} PROPERTIES
                CXX_CLANG_TIDY "${SANCAK_CLANG_TIDY_CMD}"
            )
        endif()
    endfunction()
else()
    function(sancak_enable_clang_tidy target_name)
    endfunction()
endif()

# Linux'ta POSIX thread kütüphanesi
if(UNIX)
    find_package(Threads REQUIRED)
endif()

# ============================================================================
# YENİ SAVAŞ PİPELINE'I (sancak/ namespace)
# ============================================================================

if(SANCAK_BUILD_COMBAT)

    # --- Network Layer (UDP Video + TCP Telemetry) ---
    add_library(sancak_network STATIC
        src/network/udp_video_streamer.cpp
        src/network/tcp_telemetry_server.cpp
        src/network/telemetry_sender.cpp
    )
    target_link_libraries(sancak_network PUBLIC sancak_build_options)
    if(WIN32)
        target_link_libraries(sancak_network PUBLIC ws2_32)
    endif()
    if(UNIX)
        target_link_libraries(sancak_network PUBLIC Threads::Threads)
    endif()
    sancak_enable_clang_tidy(sancak_network)

    # --- Config Manager ---
    add_library(sancak_config STATIC
        src/sancak/config_manager.cpp
    )
    target_link_libraries(sancak_config PUBLIC sancak_opencv sancak_build_options)
    sancak_enable_clang_tidy(sancak_config)

    # --- YOLO Detector ---
    add_library(sancak_yolo STATIC
        src/sancak/yolo_detector.cpp
    )
    target_link_libraries(sancak_yolo PUBLIC sancak_opencv sancak_config sancak_build_options)
    if(SANCAK_USE_ONNXRUNTIME)
        target_link_libraries(sancak_yolo PUBLIC sancak_onnxruntime)
    endif()
    sancak_enable_clang_tidy(sancak_yolo)

    # --- Balloon Segmentor ---
    add_library(sancak_balloon STATIC
        src/sancak/balloon_segmentor.cpp
    )
    target_link_libraries(sancak_balloon PUBLIC sancak_opencv sancak_config sancak_build_options)
    sancak_enable_clang_tidy(sancak_balloon)

    # --- Distance Estimator ---
    add_library(sancak_distance STATIC
        src/sancak/distance_estimator.cpp
    )
    target_link_libraries(sancak_distance PUBLIC sancak_config sancak_build_options)
    sancak_enable_clang_tidy(sancak_distance)

    # --- Ballistics Manager ---
    add_library(sancak_ballistics STATIC
        src/sancak/ballistics_manager.cpp
    )
    target_link_libraries(sancak_ballistics PUBLIC sancak_opencv sancak_config sancak_build_options)
    sancak_enable_clang_tidy(sancak_ballistics)

    # --- Aim Solver ---
    add_library(sancak_aim STATIC
        src/sancak/aim_solver.cpp
        src/sancak/trigger_controller.cpp
    )
    target_link_libraries(sancak_aim PUBLIC
        sancak_ballistics
        sancak_distance
        sancak_config
        sancak_build_options
    )
    sancak_enable_clang_tidy(sancak_aim)

    # --- Target Tracker ---
    add_library(sancak_tracker STATIC
        src/sancak/target_tracker.cpp
    )
    target_link_libraries(sancak_tracker PUBLIC sancak_opencv sancak_config sancak_build_options)
    sancak_enable_clang_tidy(sancak_tracker)

    # --- Camera Controller ---
    add_library(sancak_camera STATIC
        src/sancak/camera_controller.cpp
    )
    target_link_libraries(sancak_camera PUBLIC sancak_opencv sancak_config sancak_build_options)
    sancak_enable_clang_tidy(sancak_camera)

    # --- Serial Communication ---
    add_library(sancak_serial STATIC
        src/sancak/serial_comm.cpp
        src/turret_controller.cpp
    )
    target_link_libraries(sancak_serial PUBLIC sancak_config sancak_build_options)
    if(UNIX)
        target_link_libraries(sancak_serial PUBLIC Threads::Threads)
    endif()
    sancak_enable_clang_tidy(sancak_serial)

    # --- Combat Pipeline ---
    add_library(sancak_pipeline STATIC
        src/sancak/combat_pipeline.cpp
        src/sancak/mock_lidar.cpp
    )
    target_link_libraries(sancak_pipeline PUBLIC
        sancak_yolo
        sancak_balloon
        sancak_aim
        sancak_tracker
        sancak_camera
        sancak_serial
        sancak_network
        sancak_config
        sancak_opencv
        sancak_build_options
    )
    sancak_enable_clang_tidy(sancak_pipeline)

    # --- CORE META TARGET (tek noktadan linkleme) ---
    add_library(sancak_combat_core INTERFACE)
    target_link_libraries(sancak_combat_core INTERFACE sancak_pipeline)

    # --- ANA ÇALIŞTIRILABIILIR ---
    add_executable(sancak_combat
        src/sancak/main.cpp
    )
    target_link_libraries(sancak_combat PRIVATE sancak_combat_core)

    # Model ve config dosyalarını build dizinine kopyala
    add_custom_command(TARGET sancak_combat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:sancak_combat>/config
        COMMENT "Config dosyaları kopyalanıyor..."
    )

    # Models dizini (best.onnx'i buraya koy)
    add_custom_command(TARGET sancak_combat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:sancak_combat>/models
        COMMENT "Models dizini hazırlanıyor..."
    )

endif() # SANCAK_BUILD_COMBAT

# ============================================================================
# ESKİ MODÜLER YAPI (Geriye Uyumluluk)
# ============================================================================

if(SANCAK_BUILD_LEGACY)

    # --- Şekil Analizi ---
    add_library(shape_analyzer STATIC
        src/shape_analyzer.cpp
    )
    target_link_libraries(shape_analyzer PUBLIC sancak_opencv sancak_build_options)
    sancak_enable_clang_tidy(shape_analyzer)

    # --- Renk Filtreleme ---
    add_library(color_filter STATIC
        src/color_filter.cpp
    )
    target_link_libraries(color_filter PUBLIC sancak_opencv sancak_build_options)
    sancak_enable_clang_tidy(color_filter)

    # --- Hareket Algılama ---
    add_library(motion_detector STATIC
        src/motion_detector.cpp
    )
    target_link_libraries(motion_detector PUBLIC sancak_opencv sancak_build_options)
    sancak_enable_clang_tidy(motion_detector)

    # --- Takip Pipeline ---
    add_library(tracking_pipeline STATIC
        src/tracking_pipeline.cpp
    )
    target_link_libraries(tracking_pipeline PUBLIC 
        sancak_opencv color_filter motion_detector sancak_build_options
    )
    sancak_enable_clang_tidy(tracking_pipeline)

    # --- Balon Detector ---
    add_library(balloon_detector STATIC
        src/balloon_detector.cpp
    )
    target_link_libraries(balloon_detector PUBLIC
        sancak_opencv shape_analyzer color_filter motion_detector sancak_build_options
    )
    sancak_enable_clang_tidy(balloon_detector)

    # Legacy core
    add_library(sancak_vision_core INTERFACE)
    target_link_libraries(sancak_vision_core INTERFACE
        balloon_detector tracking_pipeline shape_analyzer
        color_filter motion_detector sancak_build_options
    )

    # Legacy executables
    add_executable(main_detector src/main_detector.cpp)
    target_link_libraries(main_detector PRIVATE sancak_vision_core)

endif() # SANCAK_BUILD_LEGACY

# ============================================================================
# KURULUM
# ============================================================================
include(GNUInstallDirs)

if(SANCAK_BUILD_COMBAT)
    install(TARGETS sancak_combat DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY config/ DESTINATION ${CMAKE_INSTALL_DATADIR}/sancak/config)
endif()

# ============================================================================
# TEST
# ============================================================================

if(SANCAK_ENABLE_TESTS AND SANCAK_BUILD_COMBAT)
    include(CTest)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(sancak_tests
        tests/test_ballistics_manager.cpp
        tests/test_distance_estimator.cpp
        tests/test_target_tracker.cpp
        tests/test_trigger_controller.cpp
        tests/test_balloon_segmentor.cpp
        tests/test_network_commands.cpp
    )
    target_link_libraries(sancak_tests PRIVATE
        sancak_combat_core
        Catch2::Catch2WithMain
    )

    target_compile_definitions(sancak_tests PRIVATE SANCAK_ENABLE_TEST_HOOKS=1)

    include(Catch)
    catch_discover_tests(sancak_tests)
endif()
