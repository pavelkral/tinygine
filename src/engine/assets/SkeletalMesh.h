#pragma once

#include "core/Types.h"
#include "engine/assets/Mesh.h"
#include "pch/Pch.h"

class SkeletalMesh {
public:
	std::vector<RHISubMesh> subMeshes;
	std::map<std::string, BoneInfo> boneInfoMap;
	int boneCounter = 0;
	std::map<std::wstring, std::shared_ptr<RHITexture>> textureCache;
	const aiScene* scene;
	Assimp::Importer importer;

	std::shared_ptr<RHIPipeline> masterSolidPipeline;
	std::shared_ptr<RHIPipeline> masterTransPipeline;
	SkeletalMesh(RHI* rhi, const std::string& path, PipelineConfig skelConfig);

private:
	void ProcessNode(RHI* rhi, aiNode* node, const aiScene* scene,
		SM::Matrix parentTransform, PipelineConfig& skelConfig,
		const std::string& dir);
};
