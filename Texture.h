
#pragma once

#include <glad/glad.h>

#include <cstddef>
#include <string>

class Texture2D
{
public:
    // GLB에 포함된 이미지 데이터
    Texture2D(const unsigned char* data, std::size_t size);

    // 외부 이미지 파일
    explicit Texture2D(const std::string& path);

    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    void bind() const;

private:
    GLuint ID = 0;

    void upload(
        const unsigned char* pixels,
        int width,
        int height
    );
};
