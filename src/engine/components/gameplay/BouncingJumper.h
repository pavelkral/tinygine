#pragma once

#include "pch/Pch.h"

class BouncingJumper : public Component {
private:
	Rigidbody* m_rb = nullptr;
	ParticleSystemComponent* m_particles = nullptr;
	float m_effectTimer = 0.0f;
	bool m_effectActive = false;

public:
	BouncingJumper();
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Start() override;
	void Update(float dt) override;
	void BeginOverlap(GameObject* other) override;
	void OnGUI() override;
};


