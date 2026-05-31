#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/PlayerShooter.h"

PlayerShooter::PlayerShooter(
	FpsCamera* camera,
	JPH::BodyInterface* physics,
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<Material> mat,
	std::function<void(std::unique_ptr<GameObject>)> spawnCallback)
	: m_camera(camera), m_physics(physics), m_bulletMesh(mesh), m_bulletMaterial(mat), m_spawnCallback(spawnCallback) {
	componentType = "PlayerShooter";
}


json PlayerShooter::Serialize() {
	json j = Component::Serialize();
	j["shootImpulse"] = m_shootImpulse;
	return j;
}


void PlayerShooter::Deserialize(const json& j) {
	m_shootImpulse = j.value("shootImpulse", 150000.0f);
}


void PlayerShooter::Update(float dt) {
	if (Input::GetMouseButtonDown(1)) {
		Shoot();
	}
}


void PlayerShooter::Shoot() {
	static int bulletCounter = 0;
	auto bullet = std::make_unique<GameObject>("Bullet_" + std::to_string(bulletCounter++));

	XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_camera->pitch, m_camera->yaw, 0);
	XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
	XMFLOAT3 fw;
	XMStoreFloat3(&fw, forward);

	bullet->transform.position = {
		m_camera->pos.x + fw.x * 2.0f,
		m_camera->pos.y + fw.y * 2.0f,
		m_camera->pos.z + fw.z * 2.0f
	};
	bullet->transform.scale = { 1.3f, 1.3f, 1.3f };

	bullet->AddComponent<MeshRenderer>(m_bulletMesh, m_bulletMaterial, true);
	bullet->AddComponent<SphereCollider>(0.55f);

	auto rb = bullet->AddComponent<Rigidbody>(m_physics, true);
	bullet->AddComponent<BulletLife>(5.0f);

	bullet->Start();
	rb->bodyInterface->AddImpulse(rb->bodyID, JPH::Vec3(fw.x * m_shootImpulse, fw.y * m_shootImpulse, fw.z * m_shootImpulse));
	if (m_spawnCallback) m_spawnCallback(std::move(bullet));
}


void PlayerShooter::OnGUI() {
	ImGui::Separator(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Player Shooter");
	ImGui::SameLine(); if (ImGui::Button("Remove##ps")) Destroy();
}
