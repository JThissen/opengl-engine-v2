#include "sandboxLayer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../imgui/imgui.h"
#include "../utils.hpp"
#include "../mouseEvent.hpp"
#include "../app.hpp"
#include "../scene/component.hpp"
#include "../windowInput.hpp"
#include <glad/glad.h>

SandboxLayer::SandboxLayer(int windowWidth, int windowHeight)
    : Layer("sandboxLayer"), viewportDimensions(glm::vec2(windowWidth, windowHeight)),
      mousePosition(glm::vec2(windowWidth / 2, windowHeight / 2)) {
  engine::FrameBufferSpecification spec;
  spec.width = viewportDimensions.x;
  spec.height = viewportDimensions.y;
  spec.textureFormats = {
    engine::FrameBufferTextureFormat::RGBA8,
    engine::FrameBufferTextureFormat::RGBA8,
    engine::FrameBufferTextureFormat::RGBA8
  };
  frameBuffer = std::make_unique<engine::FrameBuffer>(spec);
  frameBuffer_2 = std::make_unique<engine::FrameBuffer>(spec);
  frameBufferDebug_1 = std::make_unique<engine::FrameBuffer>(spec);
  openglRenderer = std::make_shared<engine::OpenglRenderer>();
  editorCamera = std::make_shared<engine::PerspectiveCamera>(viewportDimensions.x, viewportDimensions.y);
  openglRenderer->viewMatrix = editorCamera->view;
  openglRenderer->projectionMatrix = editorCamera->projection;
  openglRenderer->createCube();
  openglRenderer->createCube(glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 3.0f)));
  openglRenderer->createCube(glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, -7.0f)));
  openglRenderer->createPlane(glm::scale(glm::mat4(1.0f), glm::vec3(25.0f, 1.0f, 25.0f)));
  cascadedShadowMaps = std::make_unique<engine::CascadedShadowMaps>(openglRenderer, editorCamera);
  bloom = std::make_unique<engine::Bloom>(viewportDimensions.x, viewportDimensions.y);
  
  //TODO: texture loader class
  texture = std::make_shared<engine::OpenglTexture>("./../src/sandbox/assets/wood.jpg");

  //testing entities and components
  scene = std::make_unique<engine::Scene>(openglRenderer);
  scene->createEntity("Test entity");
  auto& textureComponent = scene->addComponent<engine::TextureComponent>();
  textureComponent.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
  textureComponent.texture = texture;
}

void SandboxLayer::update(float time, engine::DeltaTime deltaTime) {
  cascadedShadowMaps->bind();
  openglRenderer->drawAxes(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.001f, 0.0f)));
  openglRenderer->drawObjects(editorCamera->position, objectSelectedId, editorCamera->nearPlane, editorCamera->farPlane);
  openglRenderer->drawGrid(glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 0.0f, -50.0f)));
  cascadedShadowMaps->unbind();

  // DEBUG: render depth textures to quads
  openglRenderer->depthPass = false;
  frameBufferDebug_1->bind();
  openglRenderer->setClearColor(glm::vec4(0.24f, 0.24f, 0.24f, 1.0f));
  openglRenderer->clear();
  editorCamera->updateView(deltaTime, mousePosition, hasMouseEnterWindow);
  openglRenderer->modelMatrix = glm::mat4(1.0f);
  openglRenderer->drawAxes(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.001f, 0.0f)));
  openglRenderer->drawGrid(glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 0.0f, -50.0f)));
  openglRenderer->drawQuad(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 0.0f)), cascadedShadowMaps->depthTextures, 0, true);
  frameBufferDebug_1->unbind();


  frameBuffer->bind();
  openglRenderer->setClearColor(glm::vec4(0.24f, 0.24f, 0.24f, 1.0f));
  openglRenderer->clear();
  editorCamera->updateView(deltaTime, mousePosition, hasMouseEnterWindow);
  // DEBUG: 2nd view
  glm::mat4 view = glm::lookAt(glm::vec3(25.0f, 15.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), viewportDimensions.x / viewportDimensions.y, 0.1f, 1000.0f);
  openglRenderer->viewMatrix = view;
  openglRenderer->projectionMatrix = projection;
  // openglRenderer->viewMatrix = editorCamera->view;
  // openglRenderer->projectionMatrix = editorCamera->projection;
  openglRenderer->drawAxes(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.001f, 0.0f)));
  openglRenderer->drawObjects(editorCamera->position, objectSelectedId, editorCamera->nearPlane, editorCamera->farPlane);
  openglRenderer->drawGrid(glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 0.0f, -50.0f)));
  setViewportMousePosition();
  setObjectSelectedId();
  frameBuffer->unbind();

  bloom->gaussianBlur(frameBuffer->colorAttachmentIds[2]);

  frameBuffer_2->bind();
  openglRenderer->setClearColor(glm::vec4(0.24f, 0.24f, 0.24f, 1.0f));
  openglRenderer->clear();
  openglRenderer->drawQuad(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)), frameBuffer->colorAttachmentIds[0], bloom->textures[0]);
  frameBuffer_2->unbind();
}

void SandboxLayer::setViewportMousePosition() {
  auto [mouseX, mouseY] = ImGui::GetMousePos();
  mouseX -= viewportBounds[0].x;
  mouseY -= viewportBounds[0].y;

  int mx = static_cast<int>(mouseX);
  int my = static_cast<int>(mouseY);
  viewportSize = viewportBounds[1] - viewportBounds[0];
  my = viewportSize.y - my;
  viewportMousePosition = {mx, my};
}

void SandboxLayer::setObjectSelectedId() {
  if (engine::WindowInput::isMousePressed(engine::Mouse::MouseCode::ButtonLeft) && viewportMousePosition.x >= 0 && viewportMousePosition.y >= 0 && viewportMousePosition.x <= viewportSize.x && viewportMousePosition.y <= viewportSize.y)
  {
    int pixelData = frameBuffer->readPixel(1, viewportMousePosition.x, viewportMousePosition.y);
    objectSelectedId = pixelData;
  }
}

//  TODO: clean this up
void SandboxLayer::createImGuiLayout() {
  // static bool b = true;
  // ImGui::ShowDemoWindow(&b);
  static bool open = true;
  static bool opt_fullscreen = true;
  static bool opt_padding = false;
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
  // because it would be confusing to have two docking targets within each others.
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen)
  {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }
  else
  {
    dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
  // and handle the pass-thru hole, so we ask Begin() to not render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
  // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
  // all active windows docked into it will lose their parent and become undocked.
  // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
  // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
  if (!opt_padding)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace Demo", &open, window_flags);
  if (!opt_padding)
    ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // Submit the DockSpace
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
  {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }

  ImGui::Begin("Settings");
  if (ImGui::CollapsingHeader("Editor Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushID(100);
    glm::vec3 &position = editorCamera->position;
    ImGui::SliderFloat3("Position", glm::value_ptr(position), -20.0f, 20.0f);
    std::array<float, 2> pitchYaw = {editorCamera->pitch, editorCamera->yaw};
    ImGui::SliderFloat2("Pitch, Yaw", pitchYaw.data(), -360.0f, 360.0f);
    editorCamera->pitch = pitchYaw[0];
    editorCamera->yaw = pitchYaw[1];
    ImGui::SliderFloat("Field of view", &editorCamera->fov, 10.0f, 90.0f);
    ImGui::SliderFloat("Near plane", &editorCamera->nearPlane, 0.1f, 1000.0f);
    ImGui::SliderFloat("Far plane", &editorCamera->farPlane, 0.1f, 1000.0f);
    editorCamera->updateProjection();
    // ImGui::Checkbox("Depth buffer", &openglRenderer->showDepthBuffer);
    ImGui::Separator();
    ImGui::PopID();
  }
  if(ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushID(102);
    ImGui::SliderInt("Amount", &bloom->amount, 0, 100);
    ImGui::PopID();
  }
  if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImVec2 settingsMenuSize = ImGui::GetContentRegionAvail();
    if (ImGui::Button("Create...", ImVec2(settingsMenuSize.x, 20.0f)))
    {
      ImGui::OpenPopup("CreatePopup");
    }
    if (ImGui::BeginPopup("CreatePopup"))
    {
      if (ImGui::Selectable("Cube"))
      {
        openglRenderer->createCube();
      }
      ImGui::EndPopup();
    }

    if (!openglRenderer->objects.empty())
    {
      ImGuiTreeNodeFlags node_flags;
      node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      for (int i = 0; i < openglRenderer->objects.size(); i++)
      {
        ImGui::TreeNodeEx((void *)(intptr_t)i, node_flags, openglRenderer->objects[i]->name.c_str(), i);
        if (ImGui::IsItemClicked())
        {
          objectSelectedId = openglRenderer->objects[i]->id;
        }
      }
    }
  }
  ImGui::PushID(101);

  // engine::Light& light = openglRenderer->lightData->lights[0];
  // ImGui::SliderFloat3("Position", glm::value_ptr(light.position), -20.0f, 20.0f);
  // ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
  // ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f);
  ImGui::Separator();
  ImGui::PopID();

  // float ambientFactor = openglRenderer->lightData->material.ambientFactor;
  // float diffuseFactor = openglRenderer->lightData->material.diffuseFactor;
  // float specularFactor = openglRenderer->lightData->material.specularFactor;
  // float shininessFactor = openglRenderer->lightData->material.shininessFactor;
  // ImGui::SliderFloat("Ambient factor", &ambientFactor, 0.0f, 5.0f);
  // ImGui::SliderFloat("Diffuse factor", &diffuseFactor, 0.0f, 5.0f);
  // ImGui::SliderFloat("Specular factor", &specularFactor, 0.0f, 5.0f);
  // ImGui::SliderFloat("Shininess factor", &shininessFactor, 0.0f, 50.0f);
  // openglRenderer->lightData->material.ambientFactor = ambientFactor;
  // openglRenderer->lightData->material.diffuseFactor = diffuseFactor;
  // openglRenderer->lightData->material.specularFactor = specularFactor;
  // openglRenderer->lightData->material.shininessFactor = shininessFactor;
  ImGui::End();

  ImGui::Begin("Inspector");
  auto object = std::find_if(openglRenderer->objects.begin(), openglRenderer->objects.end(), [id = objectSelectedId](std::unique_ptr<engine::Object> &object)
                             { return object->id == id; });

  if (object != openglRenderer->objects.end())
  {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
      ImGui::PushID(objectSelectedId);
      glm::mat4 &modelMatrix = (*object)->transformData.modelMatrix;

      glm::vec3 positionFromMatrix = {modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2]};
      ImGui::SliderFloat3("Translate", glm::value_ptr(positionFromMatrix), -20.0f, 20.0f);
      glm::mat4 translate = glm::translate(glm::mat4(1.0f), positionFromMatrix);

      glm::vec3 &rotationRef = (*object)->transformData.rotation;
      ImGui::SliderFloat3("Rotate", glm::value_ptr(rotationRef), 0.0f, 360.0f);
      glm::quat rotateX = glm::angleAxis(glm::radians(rotationRef.x), glm::vec3(1.0f, 0.0f, 0.0f));
      glm::quat rotateY = glm::angleAxis(glm::radians(rotationRef.y), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::quat rotateZ = glm::angleAxis(glm::radians(rotationRef.z), glm::vec3(0.0f, 0.0f, 1.0f));
      glm::mat4 rotate = glm::toMat4(glm::normalize(rotateX * rotateY * rotateZ));

      glm::vec3 &scaleRef = (*object)->transformData.scale;
      ImGui::SliderFloat3("Scale", glm::value_ptr(scaleRef), -20.0f, 20.0f);
      glm::mat4 scale = glm::scale(glm::mat4(1.0f), scaleRef);
      modelMatrix = translate * rotate * scale;
      ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Shader", ImGuiTreeNodeFlags_DefaultOpen))
    {
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
    {
    }
  }
  ImGui::End();

  ImGui::Begin("Log");
  //TODO: add log
  ImGui::End();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  ImGui::Begin("Viewport");
  ImVec2 offset = ImGui::GetCursorPos();
  ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
  glm::vec2 newViewportDimensions = {contentRegionAvailable.x, contentRegionAvailable.y};
  if (viewportDimensions != newViewportDimensions) {
    frameBuffer->onResize(static_cast<int>(newViewportDimensions.x), static_cast<int>(newViewportDimensions.y));
    frameBufferDebug_1->onResize(static_cast<int>(newViewportDimensions.x), static_cast<int>(newViewportDimensions.y));
    viewportDimensions = newViewportDimensions;
  }

  // ImGui::Image(reinterpret_cast<void*>(bloom->textures[0]), {viewportDimensions.x, viewportDimensions.y - 115.0f}, {0, 1}, {1, 0});
  ImGui::Image(reinterpret_cast<void*>(frameBuffer_2->colorAttachmentIds[0]), {viewportDimensions.x, viewportDimensions.y - 115.0f}, {0, 1}, {1, 0});
  // ImGui::Image(reinterpret_cast<void*>(frameBuffer->colorAttachmentIds[0]), {viewportDimensions.x, viewportDimensions.y - 115.0f}, {0, 1}, {1, 0});
  ImGui::Image(reinterpret_cast<void*>(frameBufferDebug_1->colorAttachmentIds[0]), {192.0f, 108.0f}, {0, 1}, {1, 0});
  ImGui::SameLine();
  ImGui::Image(reinterpret_cast<void*>(frameBufferDebug_1->colorAttachmentIds[1]), {192.0f, 108.0f}, {0, 1}, {1, 0});
  ImGui::SameLine();
  ImGui::Image(reinterpret_cast<void*>(frameBufferDebug_1->colorAttachmentIds[2]), {192.0f, 108.0f}, {0, 1}, {1, 0});
  ImGui::SameLine();
  ImGui::Image(reinterpret_cast<void*>(frameBuffer->colorAttachmentIds[2]), {192.0f, 108.0f}, {0, 1}, {1, 0});

  ImGui::SameLine();
  ImGui::Image(reinterpret_cast<void*>(frameBuffer->colorAttachmentIds[0]), {192.0f, 108.0f}, {0, 1}, {1, 0});


  ImVec2 windowSize = ImGui::GetWindowSize();
  ImVec2 windowPosition = ImGui::GetWindowPos();
  windowPosition.x += offset.x;
  windowPosition.y += offset.y;
  ImVec2 maxWindowPosition = {windowPosition.x + windowSize.x, windowPosition.y + windowSize.y};
  viewportBounds[0] = {windowPosition.x, windowPosition.y};
  viewportBounds[1] = {maxWindowPosition.x, maxWindowPosition.y};
  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::End();
}

void SandboxLayer::event(engine::Event &event) {
  if (event.eventType == engine::Event::EventType::MouseScrolled)
  {
    auto mouseScrolledEvent = dynamic_cast<engine::MouseScrolledEvent &>(event);
    editorCamera->scroll(mouseScrolledEvent);
    //TODO
  }
  else if (event.eventType == engine::Event::EventType::WindowResized)
  {
    auto windowResizedEvent = dynamic_cast<engine::WindowResizedEvent &>(event);
    editorCamera->resize(windowResizedEvent);
    // TODO
  }
  else if (event.eventType == engine::Event::EventType::FramebufferResized)
  {
    // TODO
  }
  else if (event.eventType == engine::Event::EventType::MousePosition)
  {
    auto mousePositionEvent = dynamic_cast<engine::MousePositionEvent &>(event);
    mousePosition = {static_cast<float>(mousePositionEvent.x), static_cast<float>(mousePositionEvent.y)};
    hasMouseEnterWindow = true;
  }
  else if (event.eventType == engine::Event::EventType::KeyPressed)
  {
    auto keyPressedEvent = dynamic_cast<engine::KeyPressedEvent &>(event);
    if (keyPressedEvent.keyCode == engine::Key::KeyCode::L)
    {
      editorCamera->locked = !editorCamera->locked;
    }
  }
}

void SandboxLayer::push() {
  // TODO
}

void SandboxLayer::remove() {
  // TOD
}
