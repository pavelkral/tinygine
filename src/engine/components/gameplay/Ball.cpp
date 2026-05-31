#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/Ball.h"

Ball::Ball() { componentType = "Ball"; }


json Ball::Serialize() {
	json j = Component::Serialize();
	j["speed"] = speed;
	return j;
}


void Ball::Deserialize(const json& j) {
	speed = j.value("speed", 25.0f);
}


void Ball::Start() {
	rb = gameObject->GetComponent<Rigidbody>();
	if (rb && !rb->bodyID.IsInvalid()) {
		rb->bodyInterface->SetLinearVelocity(rb->bodyID, JPH::Vec3(speed * 0.7f, speed * 0.7f, 0.0f));
	}
}


void Ball::FixedUpdate(float fixedDt) {
	if (!rb || rb->bodyID.IsInvalid()) return;

	JPH::Vec3 vel = rb->bodyInterface->GetLinearVelocity(rb->bodyID);
	vel.SetZ(0.0f);

	if (abs(vel.GetY()) < 3.0f) vel.SetY(vel.GetY() >= 0.0f ? 3.0f : -3.0f);
	if (abs(vel.GetX()) < 3.0f) vel.SetX(vel.GetX() >= 0.0f ? 3.0f : -3.0f);

	float currentSpeed = vel.Length();
	if (currentSpeed > 0.01f) {
		vel = (vel / currentSpeed) * speed;
	}
	else {
		vel = JPH::Vec3(speed * 0.7f, speed * 0.7f, 0.0f);
	}

	rb->bodyInterface->SetLinearVelocity(rb->bodyID, vel);
	rb->bodyInterface->ActivateBody(rb->bodyID);
	JPH::RVec3 pos; JPH::Quat rot;
	rb->bodyInterface->GetPositionAndRotation(rb->bodyID, pos, rot);
	if (abs(pos.GetZ()) > 0.01f) {
		rb->bodyInterface->SetPosition(rb->bodyID, JPH::RVec3(pos.GetX(), pos.GetY(), 0.0f), JPH::EActivation::DontActivate);
	}
}


void Ball::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "Ball Logic");
	ImGui::SameLine();
	if (ImGui::Button("Remove##ball")) Destroy();

	ImGui::SliderFloat("Speed", &speed, 1.0f, 100.0f);
}
