#include "engine/EngineDependencies.h"
#include "engine/components/physics/SphereCollider.h"

SphereCollider::SphereCollider(float r) : radius(r) { componentType = "SphereCollider"; }


SphereCollider::SphereCollider(float r, SM::Vector3 c) : radius(r) { center = c; componentType = "SphereCollider"; }


JPH::Ref<JPH::Shape> SphereCollider::CreateShape() {
	return ApplyOffset(new JPH::SphereShape(radius));
}


json SphereCollider::Serialize() {
	json j = Component::Serialize();
	j["radius"] = radius;
	j["center"] = { center.x, center.y, center.z };
	return j;
}


void SphereCollider::Deserialize(const json& j) {
	radius = j.value("radius", 0.5f);
	if (j.contains("center")) center = { j["center"][0], j["center"][1], j["center"][2] };
}


void SphereCollider::OnShapeGUI(bool& shapeChanged) {
	if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
}
