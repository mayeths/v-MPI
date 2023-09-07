#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <vector>
#include <string>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "framework/GUI.hpp"
#include "framework/Shader.hpp"
#include "framework/Window.hpp"
#include "util/log.h"
#include "SmileBox.hpp"

int main() {
    GLint success = 0;
    Window window(&success, "A-4-LoopSubdivision", 800, 600);
    if (success != 1) {
        log_fatal("Failed to create GLFW window");
        return -1;
    }
    window.EnableStatisticGUI(Window::STAT_FPS | Window::STAT_MEMORY | Window::STAT_POSITION);

    Camera &camera = window.camera;
    {
        camera.SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
        camera.SetMovementSpeed(15);
        camera.SetMaxRenderDistance(1e8f);
    }

    std::vector<SmileBox> smileBoxs(12);
    for (int i = 0; i < smileBoxs.size(); i++) {
        smileBoxs[i].SetShaderPath("assets/shaders/smilebox.vs", "assets/shaders/smilebox.fs");
        smileBoxs[i].SetTexturePath("assets/textures/container.jpg", "assets/textures/awesomeface.png");
        smileBoxs[i].Setup(i);
        std::vector<glm::vec3> cubePositions {
            glm::vec3(0.0f * 2, -1.0f, -40.0f),
            glm::vec3(1.0f * 2, -1.0f, -40.0f),
            glm::vec3(2.0f * 2, -1.0f, -40.0f),
            glm::vec3(3.0f * 2, -1.0f, -40.0f),
            glm::vec3(0.0f * 2,  0.0f, -40.0f),
            glm::vec3(1.0f * 2,  0.0f, -40.0f),
            glm::vec3(2.0f * 2,  0.0f, -40.0f),
            glm::vec3(3.0f * 2,  0.0f, -40.0f),
            glm::vec3(0.0f * 2, +1.0f, -40.0f),
            glm::vec3(1.0f * 2, +1.0f, -40.0f),
            glm::vec3(2.0f * 2, +1.0f, -40.0f),
            glm::vec3(3.0f * 2, +1.0f, -40.0f),
        };
        assert(smileBoxs.size() <= cubePositions.size());
        smileBoxs[i].MoveTo(cubePositions[i]);
        window.AddObject(&smileBoxs[i]);
    }

    double now;
    double lastUpdateTime = 0;
    double lastRenderTime = 0;
    double FPSlimits = 60; // std::numeric_limits<double>::infinity()
    while (window.ContinueLoop()) {
        now = glfwGetTime();
        ////// Update Logic
        window.Update(now, lastUpdateTime);
        lastUpdateTime = now;
        ////// Render Frame
        if (now - lastRenderTime >= 1 / FPSlimits) {
            window.Render(now, lastRenderTime);
            lastRenderTime = now;
        }
    }

    return 0;
}