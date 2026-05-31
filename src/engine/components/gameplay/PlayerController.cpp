#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/PlayerController.h"

PlayerController::PlayerController() { componentType = "PlayerController"; }


json PlayerController::Serialize() {
	json j = Component::Serialize();
	j["speed"] = m_speed;
	return j;
}


void PlayerController::Deserialize(const json& j) {
	m_speed = j.value("speed", 500.0f);
}


void PlayerController::Start() {
	m_rb = gameObject->GetComponent<Rigidbody>();
}


void PlayerController::Update(float dt) {
	if (m_hitFlashTimer > 0.0f) {
		m_hitFlashTimer -= dt;
		if (m_hitFlashTimer <= 0.0f) {
			if (auto smr = gameObject->GetComponent<SkinnedMeshRenderer>()) {
				smr->baseColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
			}
		}
	}
}


void PlayerController::FixedUpdate(float fixedDt) {
	if (m_rb && !m_rb->bodyID.IsInvalid()) {
		m_rb->bodyInterface->AddForce(m_rb->bodyID, JPH::Vec3(0, 0, m_speed));
		m_rb->bodyInterface->SetAngularVelocity(m_rb->bodyID, JPH::Vec3(0, 0, 0));
	}
}


void PlayerController::BeginOverlap(GameObject* other) {
	if (auto smr = gameObject->GetComponent<SkinnedMeshRenderer>()) {
		smr->baseColorMultiplier = { 1.0f, 0.2f, 0.2f, 1.0f };
		m_hitFlashTimer = 0.5f;
	}
}


void PlayerController::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Player Controller");
	ImGui::SameLine();
	if (ImGui::Button("Remove##pc")) Destroy();
	ImGui::SliderFloat("Speed", &m_speed, 100.0f, 2000.0f);
}
