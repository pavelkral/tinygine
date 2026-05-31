#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/BulletLife.h"

BulletLife::BulletLife(float lifetimeSeconds) : m_timeLeft(lifetimeSeconds) { componentType = "BulletLife"; }


json BulletLife::Serialize() {
	json j = Component::Serialize();
	j["timeLeft"] = m_timeLeft;
	return j;
}


void BulletLife::Deserialize(const json& j) {
	m_timeLeft = j.value("timeLeft", 5.0f);
}


void BulletLife::Update(float dt) {
	m_timeLeft -= dt;
	if (m_timeLeft <= 0.0f) {
		gameObject->Destroy();
	}
}


void BulletLife::OnGUI() {
	ImGui::Separator(); ImGui::TextColored(ImVec4(1, 1, 0, 1), "Bullet Life");
	ImGui::SameLine(); if (ImGui::Button("Remove##bl")) Destroy();
	ImGui::Text("Time Left: %.2f s", m_timeLeft);
}
