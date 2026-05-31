#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/PlayerJumper.h"

PlayerJumper::PlayerJumper() { componentType = "PlayerJumper"; }


json PlayerJumper::Serialize() {
	json j = Component::Serialize();
	j["jumpImpulse"] = m_jumpImpulse;
	return j;
}


void PlayerJumper::Deserialize(const json& j) {
	m_jumpImpulse = j.value("jumpImpulse", 111500.0f);
}


void PlayerJumper::Start() {
	m_rb = gameObject->GetComponent<Rigidbody>();
}


void PlayerJumper::Update(float dt) {
	if (Input::GetKeyDown(VK_SPACE)) {
		if (m_rb && !m_rb->bodyID.IsInvalid()) {
			m_rb->bodyInterface->AddImpulse(m_rb->bodyID, JPH::Vec3(0.0f, m_jumpImpulse, 0.0f));
		}
	}
}


void PlayerJumper::OnGUI() {
	ImGui::Separator(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Player Jumper");
	ImGui::Text("Jump Impulse: %.0f", m_jumpImpulse);
	ImGui::SameLine(); if (ImGui::Button("Remove##pj")) Destroy();
}
