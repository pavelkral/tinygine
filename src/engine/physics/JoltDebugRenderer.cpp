#include "engine/EngineDependencies.h"
#include "engine/physics/JoltDebugRenderer.h"

JoltDebugRenderer::JoltDebugRenderer() {
	Initialize();
}


void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) {
	uint32_t c = (inColor.r) | (inColor.g << 8) | (inColor.b << 16) | (inColor.a << 24);
	m_lines.push_back({
		SM::Vector3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
		SM::Vector3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
		c
		});
}


void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) {
	DrawLine(inV1, inV2, inColor);
	DrawLine(inV2, inV3, inColor);
	DrawLine(inV3, inV1, inColor);
}


void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPos, const std::string_view& inStr, JPH::ColorArg inCol, float inH) {}
