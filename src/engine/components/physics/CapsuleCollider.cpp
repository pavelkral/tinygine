#include "engine/EngineDependencies.h"
#include "engine/components/physics/CapsuleCollider.h"
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
CapsuleCollider::CapsuleCollider(float hh, float r) : halfHeight(hh), radius(r) { componentType = "CapsuleCollider"; }


CapsuleCollider::CapsuleCollider(float hh, float r, SM::Vector3 c) : halfHeight(hh), radius(r) { center = c; componentType = "CapsuleCollider"; }


JPH::Ref<JPH::Shape> CapsuleCollider::CreateShape() {
	return ApplyOffset(new JPH::CapsuleShape(halfHeight, radius));
}


json CapsuleCollider::Serialize() {
	json j = Component::Serialize();
	j["halfHeight"] = halfHeight;
	j["radius"] = radius;
	j["center"] = { center.x, center.y, center.z };
	return j;
}


void CapsuleCollider::Deserialize(const json& j) {
	halfHeight = j.value("halfHeight", 0.5f);
	radius = j.value("radius", 0.5f);
	if (j.contains("center")) center = { j["center"][0], j["center"][1], j["center"][2] };
}


void CapsuleCollider::OnShapeGUI(bool& shapeChanged) {
	if (ImGui::DragFloat("Half Height", &halfHeight, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
	if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 100.0f)) shapeChanged = true;
}
