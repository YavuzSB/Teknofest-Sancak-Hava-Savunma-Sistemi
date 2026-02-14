#pragma once

#include <cstdint>

namespace teknofest {

/// OpenGL texture handle (GLuint)
using TextureID = unsigned int;

/**
 * @brief OpenGL texture yardımcı sınıfı
 *
 * JPEG → piksel → GPU texture yükleme işlemlerini yönetir.
 */
class TextureHelper final {
public:
    /// Boş bir GL texture oluştur (LINEAR filtre, UNPACK_ALIGNMENT=1)
    static TextureID create();

    /// Mevcut texture'ı yeni piksel verileriyle güncelle
    static void update(TextureID id, const uint8_t* data, int width, int height, int channels = 3);

    /// GL texture'ı serbest bırak
    static void destroy(TextureID id);

    TextureHelper() = delete;
};

} // namespace teknofest
