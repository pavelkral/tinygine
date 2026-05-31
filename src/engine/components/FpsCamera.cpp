#include "engine/EngineDependencies.h"
#include "engine/components/FpsCamera.h"

void FpsCamera::Update(float dt) {
	SM::Vector2 mouseDelta = Input::GetMouseDelta();

	yaw += mouseDelta.x * sensitivity;
	pitch = std::clamp(pitch + mouseDelta.y * sensitivity, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);

	XMMATRIX rot = XMMatrixRotationRollPitchYaw(pitch, yaw, 0);
	XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
	XMVECTOR right = XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), rot);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	XMVECTOR p = XMLoadFloat3(&pos);
	float s = speed * dt;

	if (Input::GetKey('W')) p += forward * s;
	if (Input::GetKey('S')) p -= forward * s;
	if (Input::GetKey('A')) p -= right * s;
	if (Input::GetKey('D')) p += right * s;
	if (Input::GetKey('E')) p += up * s;
	if (Input::GetKey('Q')) p -= up * s;

	XMStoreFloat3(&pos, p);
}


XMMATRIX FpsCamera::GetViewMatrix() const {
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(pitch, yaw, 0);
	XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
	return XMMatrixLookToLH(XMLoadFloat3(&pos), forward, XMVectorSet(0, 1, 0, 0));
}


JPH::RRayCast FpsCamera::GetMouseRay(float mouseX, float mouseY, float screenW, float screenH, Vector3d physOrigin) const {
	XMMATRIX view = GetViewMatrix();
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, screenW / screenH, 0.1f, 100.0f);

	XMVECTOR nearVec = XMVector3Unproject(XMVectorSet(mouseX, mouseY, 0.0f, 0.0f), 0, 0, screenW, screenH, 0.0f, 1.0f, proj, view, XMMatrixIdentity());
	XMVECTOR farVec = XMVector3Unproject(XMVectorSet(mouseX, mouseY, 1.0f, 0.0f), 0, 0, screenW, screenH, 0.0f, 1.0f, proj, view, XMMatrixIdentity());
	XMVECTOR dir = XMVector3Normalize(farVec - nearVec);

	XMFLOAT3 dirF;
	XMStoreFloat3(&dirF, dir);

	JPH::RRayCast ray;
	ray.mOrigin = JPH::RVec3(pos.x - static_cast<float>(physOrigin.x), pos.y - static_cast<float>(physOrigin.y), pos.z - static_cast<float>(physOrigin.z));
	ray.mDirection = JPH::Vec3(dirF.x, dirF.y, dirF.z) * 1000.0f;
	return ray;
}
