/**
 * @file TextureHelper.cpp
 * @brief OpenGL texture oluşturma / güncelleme / silme
 */
#include "TextureHelper.hpp"

#ifdef _WIN32
#   include <windows.h>
#endif
#include <GL/gl.h>

namespace teknofest {

TextureID TextureHelper::create() {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    return static_cast<TextureID>(tex);
}

void TextureHelper::update(TextureID id, const uint8_t* data,
                           int width, int height, int channels) {
    const GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(id));
    glTexImage2D(GL_TEXTURE_2D, 0,
                 static_cast<GLint>(format),
                 width, height, 0,
                 format, GL_UNSIGNED_BYTE, data);
}

void TextureHelper::destroy(TextureID id) {
    auto tex = static_cast<GLuint>(id);
    if (tex != 0) {
        glDeleteTextures(1, &tex);
    }
}

} // namespace teknofest
