#pragma once

#include "pch/Pch.h"

class PlayerShooter : public Component {
public: // /
	FpsCamera* m_camera;
	JPH::BodyInterface* m_physics;
	std::shared_ptr<Mesh> m_bulletMesh;
	std::shared_ptr<Material> m_bulletMaterial;
	std::function<void(std::unique_ptr<GameObject>)> m_spawnCallback;

	float m_shootImpulse = 150000.0f;
	PlayerShooter(
		FpsCamera* camera = nullptr,
		JPH::BodyInterface* physics = nullptr,
		std::shared_ptr<Mesh> mesh = nullptr,
		std::shared_ptr<Material> mat = nullptr,
		std::function<void(std::unique_ptr<GameObject>)> spawnCallback = nullptr);    
	json Serialize() override;    
	void Deserialize(const json& j) override;    
	void Update(float dt) override;    
	void Shoot();    
	void OnGUI() override;
};

