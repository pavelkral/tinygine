#pragma once

#include "pch/Pch.h"

class Brick : public Component {
public:
	bool isBroken = false;
	float deathTimer = 1.0f;
	ma_engine* audioEngine = nullptr;
	Brick(ma_engine* audio = nullptr);
	json Serialize() override;
	void Deserialize(const json& j) override;
	void BeginOverlap(GameObject* other) override;
	void Update(float dt) override;
};
