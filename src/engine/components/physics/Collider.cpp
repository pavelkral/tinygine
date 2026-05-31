#include "engine/EngineDependencies.h"
#include "engine/components/physics/Collider.h"
#include "engine/components/physics/Rigidbody.h"

void Collider::OnShapeGUI(bool& shapeChanged) {}

void Collider::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Collider");
	ImGui::SameLine();
	if (ImGui::Button("Remove##col")) {
		Destroy();
	}

	bool shapeChanged = false;
	if (ImGui::DragFloat3("Center Offset", &center.x, 0.1f)) {
		shapeChanged = true;
	}

	OnShapeGUI(shapeChanged);

	if (shapeChanged) {
		if (auto rb = gameObject->GetComponent<Rigidbody>()) {
			rb->RecreateShape();
		}
	}
}

JPH::Ref<JPH::Shape> Collider::ApplyOffset(JPH::Ref<JPH::Shape> baseShape) {
	if (center.x == 0.0f && center.y == 0.0f && center.z == 0.0f) return baseShape;
	JPH::RotatedTranslatedShapeSettings settings(JPH::Vec3(center.x, center.y, center.z), JPH::Quat::sIdentity(), baseShape);
	return settings.Create().Get();
}
