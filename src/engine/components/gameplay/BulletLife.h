#pragma once

#include "pch/Pch.h"

class BulletLife : public Component {
private:
	float m_timeLeft;

public:
	BulletLife(float lifetimeSeconds = 5.0f);
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Update(float dt) override;
	void OnGUI() override;
};
