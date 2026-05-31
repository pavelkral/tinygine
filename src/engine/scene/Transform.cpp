#include "engine/EngineDependencies.h"
#include "engine/scene/Transform.h"

Transform::Transform() : position(0, 0, 0), eulerAngles(0, 0, 0), rotation(SM::Quaternion::Identity), scale(SM::Vector3::One) {}


void Transform::UpdateRotation() {
	rotation = SM::Quaternion::CreateFromYawPitchRoll(
		eulerAngles.y * (XM_PI / 180.0f),
		eulerAngles.x * (XM_PI / 180.0f),
		eulerAngles.z * (XM_PI / 180.0f)
	);
}
