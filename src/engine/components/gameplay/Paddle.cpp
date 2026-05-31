#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/Paddle.h"

Paddle::Paddle() { componentType = "Paddle"; }


json Paddle::Serialize() {
	json j = Component::Serialize();
	j["speed"] = speed;
	j["mouseSensitivity"] = mouseSensitivity;
	return j;
}


void Paddle::Deserialize(const json& j) {
	speed = j.value("speed", 30.0f);
	mouseSensitivity = j.value("mouseSensitivity", 0.05f);
}


void Paddle::Start() {
	rb = gameObject->GetComponent<Rigidbody>();
	if (rb && !rb->bodyID.IsInvalid()) {
		rb->bodyInterface->SetMotionType(rb->bodyID, JPH::EMotionType::Kinematic, JPH::EActivation::Activate);
	}
}


void Paddle::Update(float dt) {
	float moveX = 0.0f;
	if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000) moveX -= 1.0f;
	if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000) moveX += 1.0f;

	if (rb && !rb->bodyID.IsInvalid()) {
		JPH::RVec3 pos; JPH::Quat rot;
		rb->bodyInterface->GetPositionAndRotation(rb->bodyID, pos, rot);

		float newX = pos.GetX() + moveX * speed * dt;
		newX += ImGui::GetIO().MouseDelta.x * mouseSensitivity;

		if (newX < -20.65f) newX = -20.65f;
		if (newX > 20.65f) newX = 20.65f;

		rb->bodyInterface->SetPosition(rb->bodyID, JPH::RVec3(newX, pos.GetY(), pos.GetZ()), JPH::EActivation::Activate);
	}

	bool tryShoot = (GetAsyncKeyState(VK_SPACE) & 0x8000) || (ImGui::IsMouseClicked(0) && !ImGui::GetIO().WantCaptureMouse);

	if (tryShoot) {
		if (!ballSpawned && spawnBallCallback) {
			JPH::RVec3 pos; JPH::Quat rot;
			rb->bodyInterface->GetPositionAndRotation(rb->bodyID, pos, rot);

			spawnBallCallback((float)pos.GetX(), (float)pos.GetY() + 2.0f);
			ballSpawned = true;
		}
	}
}


void Paddle::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Paddle Controller");
	ImGui::SameLine();
	if (ImGui::Button("Remove##pad")) Destroy();

	ImGui::SliderFloat("Speed (Keys)", &speed, 10.0f, 100.0f);
	ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 0.2f);
}
