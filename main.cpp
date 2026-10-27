
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"
#include "GltfModel.h"

#include <iostream>
#include <exception>

#include <cmath>

int main(int argc, char** argv)
{
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        1280, 720, "sunmuToon", nullptr, nullptr
    );

    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);

    int result = 0;

    try
    {
        Shader shader(
            "shaders/basic.vert",
            "shaders/basic.frag"
        );

        Camera camera;

        // 외부 GLB 모델 로딩
        std::string modelPath =
            (argc > 1)
            ? argv[1]
            : "models/test.vrm";

        std::cout << "Loading model: "
            << modelPath << '\n';

        GltfModel model(modelPath);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE)
                == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, true);
            }

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            if (width == 0 || height == 0) {
                glfwWaitEvents();
                continue;
            }

            glViewport(0, 0, width, height);

            glClearColor(0.12f, 0.15f, 0.22f, 1.0f);
            glClear(
                GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT
            );

            shader.use();

            // 모델 회전
            glm::mat4 modelMatrix(1.0f);

            float angle =
                static_cast<float>(glfwGetTime())
                * glm::radians(35.0f);

            modelMatrix = glm::rotate(
                modelMatrix,
                angle,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            float aspect =
                static_cast<float>(width) / height;

            shader.setMat4(
                "view",
                camera.getViewMatrix()
            );

            shader.setMat4(
                "projection",
                camera.getProjectionMatrix(aspect)
            );

            // GLB 모델 렌더링

            // 시간에 따라 왼팔 회전
            float time = static_cast<float>(glfwGetTime());

            float armAngle =
                glm::radians(25.0f) *
                std::sin(time * 1.5f);

            // GPU 스키닝 적용
            model.draw(shader, modelMatrix, armAngle);


            glfwSwapBuffers(window);
        }

        // GPU 객체는 여기서 해제됨
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        result = -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return result;
}
