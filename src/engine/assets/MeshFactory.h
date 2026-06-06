#pragma once

#include "pch/Pch.h"
#include "engine/assets/Mesh.h"

namespace MeshFactory {
	std::shared_ptr<Mesh> CreateCube(
		RHI* rhi,
		float size = 1.0f,
		SM::Vector2 uvScale = { 1.0f, 1.0f });

	std::shared_ptr<Mesh> CreateSphere(
		RHI* rhi,
		float radius = 0.5f,
		int resolution = 20,
		SM::Vector2 uvScale = { 1.0f, 1.0f });

	std::shared_ptr<Mesh> CreateCapsule(
		RHI* rhi,
		float radius = 0.5f,
		float halfHeight = 0.5f,
		int resolution = 16,
		SM::Vector2 uvScale = { 1.0f, 1.0f });

	std::shared_ptr<Mesh> CreatePlane(
		RHI* rhi,
		float width = 10.0f,
		float depth = 10.0f,
		int resX = 10,
		int resZ = 10,
		SM::Vector2 uvScale = { 1.0f, 1.0f });
}
