
#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"

#include <limits>
#include <stdexcept>

Texture2D::Texture2D(
    const unsigned char* data,
    std::size_t size)
{
    if (!data ||
        size > static_cast<std::size_t>(
            std::numeric_limits<int>::max()))
        throw std::runtime_error("Invalid image data");

    // glTF UV 좌표계에 맞게 이미지 방향 변환
    stbi_set_flip_vertically_on_load(false);

    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* pixels = stbi_load_from_memory(
        data,
        static_cast<int>(size),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels)
        throw std::runtime_error("Image decoding failed");

    upload(pixels, width, height);
    stbi_image_free(pixels);
}

Texture2D::Texture2D(const std::string& path)
{
    stbi_set_flip_vertically_on_load(false);

    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* pixels = stbi_load(
        path.c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels)
        throw std::runtime_error(
            "Cannot load texture: " + path
        );

    upload(pixels, width, height);
    stbi_image_free(pixels);
}

void Texture2D::upload(
    const unsigned char* pixels,
    int width,
    int height)
{
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_REPEAT
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_REPEAT
    );

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::bind() const
{
    glBindTexture(GL_TEXTURE_2D, ID);
}

Texture2D::~Texture2D()
{
    if (ID != 0)
        glDeleteTextures(1, &ID);
}
