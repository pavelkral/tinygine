#include "engine/EngineDependencies.h"
#include "engine/assets/SkeletalMesh.h"
#include "engine/core/DebugAssimp.h"
SkeletalMesh::SkeletalMesh(RHI* rhi, const std::string& path, PipelineConfig skelConfig) {
	masterSolidPipeline = rhi->CreatePipeline(skelConfig);
	PipelineConfig transCfg = skelConfig;
	transCfg.cullMode = CullMode::None;
	transCfg.isTransparent = true;
	transCfg.depthWrite = false;
	masterTransPipeline = rhi->CreatePipeline(transCfg);

	if (path.empty() || !fs::exists(path)) {
		std::cerr << "[SkeletalMesh] Error: Path is empty or file does not exist: " << path << "\n";
		scene = nullptr;
		return;
	}

	scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "[SkeletalMesh] ASSIMP ERROR: " << importer.GetErrorString() << "\n"; return;
	}


	std::string normalizedPath = path;
	std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

	std::string directory = "";
	size_t lastSlash = normalizedPath.find_last_of('/');
	if (lastSlash != std::string::npos) {
		directory = normalizedPath.substr(0, lastSlash);
	}
	DebugPrintAssimpScene(scene, directory);
	ProcessNode(rhi, scene->mRootNode, scene, SM::Matrix::Identity, skelConfig, directory);
}


void SkeletalMesh::ProcessNode(RHI* rhi, aiNode* node, const aiScene* scene, SM::Matrix parentTransform, PipelineConfig& skelConfig, const std::string& dir) {
	SM::Matrix nodeTransform = AssimpToSimpleMathMatrix(node->mTransformation) * parentTransform;

	for (unsigned int m = 0; m < node->mNumMeshes; m++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[m]];
		RHISubMesh subMesh; subMesh.name = mesh->mName.C_Str();

		std::vector<SkinnedVertex> vertices(mesh->mNumVertices);
		std::vector<uint32_t> indices;

		bool hasBones = mesh->mNumBones > 0;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
			SM::Vector3 pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
			SM::Vector3 norm = { 0, 1, 0 };
			if (mesh->HasNormals()) norm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

			if (!hasBones) { pos = SM::Vector3::Transform(pos, nodeTransform); norm = SM::Vector3::TransformNormal(norm, nodeTransform); }

			vertices[i].pos = { pos.x, pos.y, pos.z }; vertices[i].normal = { norm.x, norm.y, norm.z };
			if (mesh->mTextureCoords[0]) vertices[i].uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y }; else vertices[i].uv = { 0.0f, 0.0f };

			for (int j = 0; j < MAX_BONE_INFLUENCE; j++) { vertices[i].boneIDs[j] = MAX_BONES - 1; vertices[i].boneWeights[j] = 0.0f; }
			if (!hasBones) vertices[i].boneWeights[0] = 1.0f;
		}

		if (hasBones) {
			for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
				int boneID = -1; std::string boneName = mesh->mBones[b]->mName.C_Str();
				if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
					BoneInfo newBoneInfo; newBoneInfo.id = boneCounter; newBoneInfo.offsetMatrix = AssimpToSimpleMathMatrix(mesh->mBones[b]->mOffsetMatrix);
					boneInfoMap[boneName] = newBoneInfo; boneID = boneCounter++;
				}
				else { boneID = boneInfoMap[boneName].id; }

				auto weights = mesh->mBones[b]->mWeights;
				for (unsigned int w = 0; w < mesh->mBones[b]->mNumWeights; ++w) {
					int vertexId = weights[w].mVertexId; float weight = weights[w].mWeight;
					for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
						if (vertices[vertexId].boneWeights[i] == 0.0f) { vertices[vertexId].boneIDs[i] = boneID; vertices[vertexId].boneWeights[i] = weight; break; }
					}
				}
			}
		}

		for (unsigned int i = 0; i < vertices.size(); i++) {
			float weightSum = 0.0f; for (int j = 0; j < MAX_BONE_INFLUENCE; j++) weightSum += vertices[i].boneWeights[j];
			if (weightSum == 0.0f) { vertices[i].boneIDs[0] = MAX_BONES - 1; vertices[i].boneWeights[0] = 1.0f; }
			else { for (int j = 0; j < MAX_BONE_INFLUENCE; j++) vertices[i].boneWeights[j] /= weightSum; }
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; i++) { for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; j++) indices.push_back(mesh->mFaces[i].mIndices[j]); }

		subMesh.vb = rhi->CreateBuffer(BufferType::Vertex, vertices.data(), vertices.size() * sizeof(SkinnedVertex), sizeof(SkinnedVertex));
		subMesh.ib = rhi->CreateBuffer(BufferType::Index, indices.data(), indices.size() * sizeof(uint32_t));
		subMesh.indexCount = (UINT)indices.size();

		subMesh.vertices = vertices;
		subMesh.indices = indices;

		if (mesh->mMaterialIndex >= 0) {
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			float opacity = 1.0f; aiGetMaterialFloat(material, AI_MATKEY_OPACITY, &opacity);

			std::string matName = material->GetName().C_Str();
			std::string matNameLower = matName;
			std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
			if ((matNameLower.find("glas") != std::string::npos) && opacity >= 1.0f) {
				opacity = 0.5f;
			}

			std::shared_ptr<RHIPipeline> sharedPipe = (opacity < 1.0f) ? masterTransPipeline : masterSolidPipeline;
			subMesh.material = std::make_shared<Material>("FBX_" + matName, sharedPipe, "Skinned");

			auto LoadTexCached = [&](const char* texPath, std::string& outPath) -> std::shared_ptr<RHITexture> {
				std::string texStr = texPath;
				std::replace(texStr.begin(), texStr.end(), '\\', '/');

				fs::path p(texStr);
				std::string filename = p.filename().string();
				std::string fullPath = dir + "/" + texStr;

				if (!fs::exists(fullPath)) fullPath = dir + "/" + filename;
				if (!fs::exists(fullPath)) fullPath = dir + "/textures/" + filename;
				if (!fs::exists(fullPath)) fullPath = dir + "/Textures/" + filename;

				if (fs::exists(fullPath)) {
					outPath = fullPath;
					std::wstring wPath(fullPath.begin(), fullPath.end());
					if (textureCache.find(wPath) == textureCache.end()) {
						auto t = rhi->CreateTexture(wPath);
						if (!t) t = rhi->CreateDDSTexture(wPath);
						textureCache[wPath] = t;
					}
					return textureCache[wPath];
				}
				outPath = "";
				return nullptr;
				};

			aiString str;
			if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
				subMesh.material->albedoTex = LoadTexCached(str.C_Str(), subMesh.material->albedoPath);
			}
			if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
				material->GetTexture(aiTextureType_NORMALS, 0, &str);
				subMesh.material->normalTex = LoadTexCached(str.C_Str(), subMesh.material->normalPath);
			}

			aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f); aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
			subMesh.material->baseColor = { color.r, color.g, color.b, opacity };
		}
		subMeshes.push_back(subMesh);
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) ProcessNode(rhi, node->mChildren[i], scene, nodeTransform, skelConfig, dir);
}
