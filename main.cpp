
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"
#include "GltfModel.h"
#include "Animator.h"
#include "FaceAnimator.h"
#include "MicrophoneInput.h"

#include <iostream>
#include <exception>

#include <string>

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

        // 모델을 더 가까이, 화면 중앙에 배치
        camera.position = glm::vec3(0.8f, 1.35f, 2.0f);
        camera.target = glm::vec3(0.0f, 1.0f, 0.0f);

        // 외부 GLB 모델 로딩
        std::string modelPath =
            (argc > 1)
            ? argv[1]
            : "models/test.vrm";

        std::cout << "Loading model: "
            << modelPath << '\n';

        GltfModel model(modelPath);
        Animator animator;
        FaceAnimator faceAnimator;


        // ===== 마이크 초기화 =====
        MicrophoneInput microphone;

        // 기본 마이크 사용 시작
        bool microphoneEnabled = microphone.start();

        if (microphoneEnabled)
        {
            std::cout << "Microphone capture started\n";
        }
        else
        {
            std::cerr
                << "Microphone initialization failed. "
                << "Manual A-key lip sync is still available.\n";
        }

        // M키의 중복 입력을 방지
        bool previousMPressed = false;


        float lastTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();


            // Delta Time
            float now = static_cast<float>(glfwGetTime());
            float deltaTime = now - lastTime;
            lastTime = now;


            // ===== 마이크 ON / OFF =====

            const bool mPressed =
                glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;

            // M키를 새로 누른 순간에만 실행
            if (mPressed && !previousMPressed)
            {
                if (microphoneEnabled)
                {
                    // 마이크 캡처까지 완전히 중지
                    microphone.stop();
                    microphoneEnabled = false;

                    std::cout << "Microphone OFF\n";
                }
                else
                {
                    microphoneEnabled = microphone.start();

                    if (microphoneEnabled)
                    {
                        std::cout << "Microphone ON\n";
                    }
                    else
                    {
                        std::cerr
                            << "Failed to start microphone\n";
                    }
                }
            }

            previousMPressed = mPressed;


            // X=Pitch(상하), Y=Yaw(좌우), Z=Roll(기울기)
            glm::vec3 targetHeadDegrees(0.0f);

            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
                targetHeadDegrees.y += 25.0f;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
                targetHeadDegrees.y -= 25.0f;
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                targetHeadDegrees.x += 15.0f;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                targetHeadDegrees.x -= 15.0f;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                targetHeadDegrees.z += 12.0f;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                targetHeadDegrees.z -= 12.0f;

            const bool waving =
                glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

            animator.update(
                model.getSkeleton(),
                deltaTime,
                targetHeadDegrees.y,  // Yaw
                targetHeadDegrees.x,  // Pitch
                targetHeadDegrees.z,  // Roll
                waving
            );

            // ===== 얼굴 애니메이션 =====

            const bool blinkPressed =
                glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;

            const bool mouthPressed =
                glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;

            const bool happyPressed =
                glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;


            // 마이크가 꺼져 있으면 음량은 0
            const float micMouthWeight =
                microphoneEnabled
                ? microphone.getMouthWeight()
                : 0.0f;

            // 자동 깜빡임 + 키보드 + 마이크 입력 처리
            const FaceWeights face = faceAnimator.update(
                deltaTime,
                blinkPressed,
                mouthPressed,
                happyPressed,
                micMouthWeight
            );

            // 기존 Morph Target 갱신
            model.updateFace(
                face.blink,
                face.mouth,
                face.happy
            );

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

            // 키보드 입력 테스트 중에는 모델 자체의 자동 회전을 끕니다.
            glm::mat4 modelMatrix(1.0f);

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

            // Animator가 Skeleton에 적용한 자세를 렌더링
            model.draw(shader, modelMatrix);

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
