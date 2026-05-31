#include "engine/EngineDependencies.h"
#include "engine/components/physics/MeshCollider.h"

MeshCollider::MeshCollider(std::shared_ptr<Mesh> mesh, std::shared_ptr<SkeletalMesh> skelMesh, std::string mName, bool convex)
	: collisionMesh(mesh), collisionSkelMesh(skelMesh), meshName(mName), isConvex(convex) {
	componentType = "MeshCollider";
}


json MeshCollider::Serialize() {
	json j = Component::Serialize();
	j["meshName"] = meshName;
	j["isConvex"] = isConvex;
	j["center"] = { center.x, center.y, center.z };
	j["hullTolerance"] = hullTolerance;
	j["maxConvexRadius"] = maxConvexRadius;
	return j;
}


void MeshCollider::Deserialize(const json& j) {
	meshName = j.value("meshName", "");
	isConvex = j.value("isConvex", false);
	if (j.contains("center")) center = { j["center"][0], j["center"][1], j["center"][2] };
	hullTolerance = j.value("hullTolerance", 0.001f);
	maxConvexRadius = j.value("maxConvexRadius", 0.05f);
}


JPH::Ref<JPH::Shape> MeshCollider::CreateShape() {
	std::vector<JPH::Vec3> physVertices;
	std::vector<uint32_t> physIndices;

	SM::Vector3 scale = gameObject ? gameObject->transform.scale : SM::Vector3(1.0f, 1.0f, 1.0f);

	if (collisionMesh && !collisionMesh->vertices.empty()) {
		physVertices.reserve(collisionMesh->vertices.size());
		for (const auto& v : collisionMesh->vertices) {
			physVertices.push_back(JPH::Vec3(v.pos.x * scale.x, v.pos.y * scale.y, v.pos.z * scale.z));
		}
		physIndices = collisionMesh->indices;
	}
	else if (collisionSkelMesh && !collisionSkelMesh->subMeshes.empty()) {
		uint32_t indexOffset = 0;
		for (const auto& sub : collisionSkelMesh->subMeshes) {
			for (const auto& v : sub.vertices) {
				physVertices.push_back(JPH::Vec3(v.pos.x * scale.x, v.pos.y * scale.y, v.pos.z * scale.z));
			}
			for (const auto& i : sub.indices) physIndices.push_back(i + indexOffset);
			indexOffset += static_cast<uint32_t>(sub.vertices.size());
		}
	}
	else {
		std::cerr << "[MeshCollider] error: no mesh loaded for generating shape cube: " << meshName << "\n";
		return ApplyOffset(new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)));
	}

	if (isConvex) {
		JPH::ConvexHullShapeSettings settings(physVertices.data(), static_cast<int>(physVertices.size()));

		// --- SETTING HULL DETAIL ---
		settings.mHullTolerance = hullTolerance;
		settings.mMaxConvexRadius = maxConvexRadius;

		JPH::ShapeSettings::ShapeResult result = settings.Create();

		if (result.HasError()) {
			std::cerr << "[MeshCollider] Convex Hull Chyba: " << result.GetError().c_str() << "\n";
			return ApplyOffset(new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)));
		}
		return ApplyOffset(result.Get());
	}
	else {
		JPH::TriangleList triangles;
		triangles.reserve(physIndices.size() / 3);
		for (size_t i = 0; i < physIndices.size(); i += 3) {
			triangles.push_back(JPH::Triangle(
				physVertices[physIndices[i]],
				physVertices[physIndices[i + 1]],
				physVertices[physIndices[i + 2]]
			));
		}
		JPH::MeshShapeSettings settings(triangles);
		JPH::ShapeSettings::ShapeResult result = settings.Create();

		if (result.HasError()) {
			std::cerr << "[MeshCollider] Mesh Shape Chyba: " << result.GetError().c_str() << "\n";
			return ApplyOffset(new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)));
		}
		return ApplyOffset(result.Get());
	}
}


void MeshCollider::OnShapeGUI(bool& shapeChanged) {
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Mesh: %s", meshName.empty() ? "None" : meshName.c_str());

	if (ImGui::Checkbox("Is Convex (Pro Dynamic/Pohyb)", &isConvex)) {
		shapeChanged = true;
	}

	// Show optimization sliders only when Convex is enabled
	if (isConvex) {
		ImGui::Indent();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Convex Detail Settings");

		if (ImGui::DragFloat("Tolerance (m)", &hullTolerance, 0.005f, 0.001f, 1.0f, "%.4f")) {
			shapeChanged = true; // Thanks to this, the shape is immediately redrawn when dragged!
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Higher tolerance = fewer polygons (massive FPS boost).");

		if (ImGui::DragFloat("Convex Radius", &maxConvexRadius, 0.005f, 0.0f, 1.0f, "%.4f")) {
			shapeChanged = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Larger radius rounds corners = smoother bounces.");

		ImGui::Unindent();
	}
}
