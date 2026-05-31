#pragma once

#include "pch/Pch.h"

class RotatingObstacle : public Component {
private:
  Rigidbody *m_rb = nullptr;
  float m_rotationSpeed = 2.0f;

public:
  RotatingObstacle();
  json Serialize() override;
  void Deserialize(const json &j) override;
  void Start() override;
  void FixedUpdate(float fixedDt) override;
  void OnGUI() override;
};
