#ifndef SANDBOX_H
#define SANDBOX_H

#include "engine.hpp"
#include "testLayer.hpp"

class Sandbox : public engine::App {
  public:
  Sandbox(bool useImGui);
  ~Sandbox() = default;
};

#endif
