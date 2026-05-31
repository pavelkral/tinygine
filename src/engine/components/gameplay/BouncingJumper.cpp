#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/BouncingJumper.h"

BouncingJumper::BouncingJumper() { componentType = "BouncingJumper"; }


json BouncingJumper::Serialize() { return Component::Serialize(); }


void BouncingJumper::Deserialize(const json& j) {}


void BouncingJumper::Start() {
	m_rb = gameObject->GetComponent<Rigidbody>();
	m_particles = gameObject->GetComponent<ParticleSystemComponent>();
	if (m_particles) m_particles->Stop();
}


void BouncingJumper::Update(float dt) {
	if (m_effectTimer > 0.0f) {
		m_effectTimer -= dt;
	}
}


void BouncingJumper::BeginOverlap(GameObject* other) {
	if (m_rb && !m_rb->bodyID.IsInvalid()) {
		m_rb->bodyInterface->AddImpulse(m_rb->bodyID, JPH::Vec3(0, 3000.0f, 0));
		if (m_particles && !m_effectActive) {
			m_effectTimer = 0.5f;
			m_effectActive = true;
		}
	}
}


void BouncingJumper::OnGUI() {
	ImGui::Separator(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Bouncing Jumper");
	ImGui::SameLine(); if (ImGui::Button("Remove##bj")) Destroy();
}
