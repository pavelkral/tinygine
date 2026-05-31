#pragma once

#include "pch/Pch.h"

class Ball : public Component {
public:
	float speed = 25.0f;
	Rigidbody* rb = nullptr;
	Ball();
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Start() override;
	void FixedUpdate(float fixedDt) override;
	void OnGUI() override;
};
