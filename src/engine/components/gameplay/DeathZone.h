#pragma once

#include "pch/Pch.h"

class DeathZone : public Component {
public:
	Paddle* paddleRef = nullptr;
	DeathZone();
	json Serialize() override;
	void Deserialize(const json& j) override;
	void BeginOverlap(GameObject* other) override;
};

/// ///////////////////////////////////////////////////////////////////////////
