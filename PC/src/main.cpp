/**
 * @file main.cpp
 * @brief Teknofest Sancak GCS – Dear ImGui + GLFW + OpenGL3 giriş noktası
 */

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <winsock2.h>
#endif

#include "App.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

// ─── GLFW hata callback ────────────────────────────────────────────────────
static void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

// ─── main ──────────────────────────────────────────────────────────────────
int main(int /*argc*/, char* /*argv*/[]) {

    // ── Winsock başlat ──────────────────────────────────────────────────────
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::fprintf(stderr, "WSAStartup basarisiz\n");
        return 1;
    }
#endif

    // ── GLFW başlat ─────────────────────────────────────────────────────────
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "GLFW baslatma hatasi\n");
        return 1;
    }

    // OpenGL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "Teknofest Sancak GCS", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Pencere olusturulamadi\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);          // VSync
    glfwMaximizeWindow(window);   // Tam ekran başlat

    // ── ImGui bağlamı ───────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // DPI ölçekleme
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    const float fontSize = 18.0f * xscale;

    // Sistem fontu (Segoe UI) – bulunamazsa ImGui varsayılanı kullanılır
#ifdef _WIN32
    {
        const char* fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        if (std::filesystem::exists(fontPath)) {
            io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
        }
    }
#endif

    // ── Backend başlat ──────────────────────────────────────────────────────
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // ── Uygulama başlat ─────────────────────────────────────────────────────
    teknofest::App app;
    if (!app.initialize()) {
        std::fprintf(stderr, "Uygulama baslatma hatasi\n");
        return 1;
    }

    // ── Ana döngü ───────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window) && !app.shouldQuit()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render(window);

        ImGui::Render();

        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.102f, 0.110f, 0.125f, 1.0f);   // #1a1c20
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ── Temizlik ────────────────────────────────────────────────────────────
    app.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
