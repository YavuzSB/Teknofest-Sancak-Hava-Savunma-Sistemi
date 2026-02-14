cmake_minimum_required(VERSION 3.21)
project(TeknofestGCS VERSION 1.0.0 LANGUAGES C CXX)

# ─── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Static Runtime (tek .exe, harici MSVC DLL gerektirmez) ──────────────────
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ─── Build Options ───────────────────────────────────────────────────────────
option(TEKNOFEST_CONSOLE "Konsol penceresi goster (debug icin)" ON)

include(FetchContent)

# ─── GLFW (statik) ──────────────────────────────────────────────────────────
set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS       OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL          OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# ─── Dear ImGui (docking branch) ────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)

# ─── stb (stb_image.h – JPEG decode) ────────────────────────────────────────
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw imgui stb)

# ─── ImGui static library ───────────────────────────────────────────────────
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC glfw)

if(WIN32)
    target_link_libraries(imgui_lib PUBLIC opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()

# ─── Application Sources ────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/App.cpp
    src/VideoReceiver.cpp
    src/TelemetryClient.cpp
    src/ArduinoController.cpp
    src/TextureHelper.cpp
)

set(HEADERS
    include/App.hpp
    include/VideoReceiver.hpp
    include/TelemetryClient.hpp
    include/ArduinoController.hpp
    include/TextureHelper.hpp
)

# ─── Executable ──────────────────────────────────────────────────────────────
if(TEKNOFEST_CONSOLE)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
else()
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup"
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui_lib)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# ─── Compiler Warnings ──────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── Install ─────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
