#include "engine/EngineDependencies.h"
#include "engine/components/physics/BoxCollider.h"

BoxCollider::BoxCollider(SM::Vector3 s) : size(s) { componentType = "BoxCollider"; }


BoxCollider::BoxCollider(SM::Vector3 s, SM::Vector3 c) : size(s) { center = c; componentType = "BoxCollider"; }


JPH::Ref<JPH::Shape> BoxCollider::CreateShape() {
	return ApplyOffset(new JPH::BoxShape(JPH::Vec3(size.x / 2.0f, size.y / 2.0f, size.z / 2.0f)));
}


json BoxCollider::Serialize() {
	json j = Component::Serialize();
	j["size"] = { size.x, size.y, size.z };
	j["center"] = { center.x, center.y, center.z };
	return j;
}


void BoxCollider::Deserialize(const json& j) {
	if (j.contains("size")) size = { j["size"][0], j["size"][1], j["size"][2] };
	if (j.contains("center")) center = { j["center"][0], j["center"][1], j["center"][2] };
}


void BoxCollider::OnShapeGUI(bool& shapeChanged) {
	if (ImGui::DragFloat3("Box Size", &size.x, 0.1f, 0.01f, 100.0f)) shapeChanged = true;
}
