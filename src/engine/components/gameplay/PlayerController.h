#pragma once

#include "pch/Pch.h"

class PlayerController : public Component {
private:
  Rigidbody *m_rb = nullptr;
  float m_speed = 500.0f;
  float m_hitFlashTimer = 0.0f;

public:
  PlayerController();
  json Serialize() override;
  void Deserialize(const json &j) override;
  void Start() override;
  void Update(float dt) override;
  void FixedUpdate(float fixedDt) override;
  void BeginOverlap(GameObject *other) override;
  void OnGUI() override;
};
