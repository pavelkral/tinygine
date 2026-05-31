#pragma once

#include "pch/Pch.h"

class Paddle : public Component {
public:
	float speed = 30.0f;
	float mouseSensitivity = 0.05f;
	bool ballSpawned = false;
	Rigidbody* rb = nullptr;
	std::function<void(float, float)> spawnBallCallback;
	Paddle();
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Start() override;
	void Update(float dt) override;
	void OnGUI() override;
};
