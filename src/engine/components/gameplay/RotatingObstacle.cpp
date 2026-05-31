#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/RotatingObstacle.h"

RotatingObstacle::RotatingObstacle() { componentType = "RotatingObstacle"; }


json RotatingObstacle::Serialize() {
	json j = Component::Serialize();
	j["rotationSpeed"] = m_rotationSpeed;
	return j;
}


void RotatingObstacle::Deserialize(const json& j) {
	m_rotationSpeed = j.value("rotationSpeed", 2.0f);
}


void RotatingObstacle::Start() {
	m_rb = gameObject->GetComponent<Rigidbody>();
}


void RotatingObstacle::FixedUpdate(float fixedDt) {
	if (m_rb && !m_rb->bodyID.IsInvalid()) {
		m_rb->bodyInterface->SetAngularVelocity(m_rb->bodyID, JPH::Vec3(0, m_rotationSpeed, 0));
	}
}


void RotatingObstacle::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Rotating Obstacle");
	ImGui::SameLine();
	if (ImGui::Button("Remove##ro")) Destroy();
	ImGui::SliderFloat("Rotation Speed", &m_rotationSpeed, -10.0f, 10.0f);
}
