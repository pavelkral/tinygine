#include "engine/EngineDependencies.h"
#include "engine/AssetRegistry.h"

void AssetRegistry::Init(RHI* rhi) {
	m_rhi = rhi;
	fs::create_directories("assets/materials");
	fs::create_directories("assets/meshes");
	fs::create_directories("assets/scenes");

	PipelineConfig defCfg;
	defCfg.vsPath = L"shaders/rhi/pbr-single.vert.hlsl"; defCfg.psPath = L"shaders/rhi/pbr.frag.hlsl"; defCfg.useInstancing = false; defCfg.numRenderTargets = 3;
	m_pipeSolid = m_rhi->CreatePipeline(defCfg);

	PipelineConfig instCfg;
	instCfg.vsPath = L"shaders/rhi/pbr.vert.hlsl"; instCfg.psPath = L"shaders/rhi/pbr.frag.hlsl"; instCfg.useInstancing = true; instCfg.numRenderTargets = 3;
	m_pipeInstanced = m_rhi->CreatePipeline(instCfg);

	PipelineConfig skelCfg;
	skelCfg.vsPath = L"shaders/rhi/pbr-skinned.vert.hlsl"; skelCfg.psPath = L"shaders/rhi/pbr-skinned.frag.hlsl"; skelCfg.useInstancing = false; skelCfg.isSkinned = true; skelCfg.numRenderTargets = 3;
	m_pipeSkinned = m_rhi->CreatePipeline(skelCfg);
}


void AssetRegistry::Clear() {
	m_meshes.clear();
	m_skelMeshes.clear();
	m_allMaterials.clear();
}


void AssetRegistry::LoadHardcodedAssets() {
	m_meshes["Sphere"] = MeshFactory::CreateSphere(m_rhi, 0.5f, 32);
	m_meshes["Cube"] = MeshFactory::CreateCube(m_rhi, 1.0f);
	m_meshes["Capsule"] = MeshFactory::CreateCapsule(m_rhi, 0.5f, 0.5f, 16);
	m_meshes["FloorCube"] = MeshFactory::CreateCube(m_rhi, 1.0f, { 50.0f, 50.0f });
	m_meshes["Plane"] = MeshFactory::CreatePlane(m_rhi, 10.0f, 10.0f, 10, 10, { 1.0f, 1.0f });

	auto matFloor = std::make_shared<Material>("Mat_Static_Floor", m_pipeSolid, "Solid");
	matFloor->albedoPath = "assets/textures/brick/brickA.png";
	matFloor->normalPath = "assets/textures/brick/brickN.png";
	matFloor->metalPath = "assets/textures/brick/brickM.png";
	matFloor->roughPath = "assets/textures/brick/brickR.png";
	matFloor->Deserialize(matFloor->Serialize(), m_rhi);
	m_allMaterials[matFloor->name] = matFloor;

	auto matWall = std::make_shared<Material>("Mat_Wall", m_pipeInstanced, "Instanced");
	matWall->baseColor = { 0.2f, 0.2f, 0.2f, 1.0f }; matWall->roughness = 0.8f; matWall->metalness = 0.5f;
	m_allMaterials[matWall->name] = matWall;

	auto matPaddle = std::make_shared<Material>("Mat_Paddle", m_pipeInstanced, "Instanced");
	matPaddle->baseColor = { 0.0f, 0.5f, 1.0f, 1.0f }; matPaddle->roughness = 0.2f; matPaddle->metalness = 0.8f;
	m_allMaterials[matPaddle->name] = matPaddle;

	auto matBall = std::make_shared<Material>("Mat_Ball", m_pipeInstanced, "Instanced");
	matBall->baseColor = { 2.0f, 2.0f, 2.0f, 1.0f }; matBall->roughness = 0.1f; matBall->metalness = 0.0f;
	m_allMaterials[matBall->name] = matBall;

	auto matMirror = std::make_shared<Material>("Mat_Mirror", m_pipeInstanced, "Instanced");
	matMirror->baseColor = { 0.02f, 0.02f, 0.02f, 1.0f }; matMirror->roughness = 0.03f; matMirror->metalness = 1.0f;
	m_allMaterials[matMirror->name] = matMirror;

	// --- SKINNED MATERI�LY ---
	auto customMatScifi = std::make_shared<Material>("Mat_Skinned_ScifiMarine", m_pipeSkinned, "Skinned");
	customMatScifi->albedoPath = "assets/models/Player/Textures/Player_D.tga";
	customMatScifi->normalPath = "assets/models/Player/Textures/Player_NRM.tga";
	customMatScifi->roughness = 0.5f; customMatScifi->metalness = 0.5f;
	customMatScifi->Deserialize(customMatScifi->Serialize(), m_rhi); // Vynut� ihned na�ten� z disku
	m_allMaterials[customMatScifi->name] = customMatScifi;

	auto customMatUS1 = std::make_shared<Material>("Mat_Skinned_USMarineBody", m_pipeSkinned, "Skinned");
	customMatUS1->albedoPath = "assets/models/USMarines/usmarine-01.jpg";
	customMatUS1->roughness = 0.5f; customMatUS1->metalness = 0.5f;
	customMatUS1->Deserialize(customMatUS1->Serialize(), m_rhi);
	m_allMaterials[customMatUS1->name] = customMatUS1;

	auto customMatUS2 = std::make_shared<Material>("Mat_Skinned_USMarineGun", m_pipeSkinned, "Skinned");
	customMatUS2->albedoPath = "assets/models/USMarines/m16.jpg";
	customMatUS2->roughness = 0.5f; customMatUS2->metalness = 0.5f;
	customMatUS2->Deserialize(customMatUS2->Serialize(), m_rhi);
	m_allMaterials[customMatUS2->name] = customMatUS2;

	// --- BRICK MATERI�LY ---
	std::vector<XMFLOAT4> colors = { {1,0,0,1}, {1,0.5,0,1}, {1,1,0,1}, {0,1,0,1} };
	for (int i = 0; i < 4; i++) {
		std::string bName = "Mat_Brick_" + std::to_string(i);
		auto m = std::make_shared<Material>(bName, m_pipeInstanced, "Instanced");
		m->baseColor = colors[i]; m->roughness = 0.5f; m->metalness = 0.1f;
		m_allMaterials[m->name] = m;
	}
}


void AssetRegistry::LoadDiskAssets() {
	if (!fs::exists("assets/materials")) return;
	for (const auto& entry : fs::directory_iterator("assets/materials")) {
		if (entry.path().extension() == ".mat") {
			std::ifstream file(entry.path());
			if (file.is_open()) {
				json j; file >> j; file.close();
				std::string name = j.value("name", "UnknownMat");
				std::string pType = j.value("pipelineType", "Instanced");

				if (m_allMaterials.find(name) != m_allMaterials.end()) {
					m_allMaterials[name]->Deserialize(j, m_rhi);
					std::cout << "[AssetRegistry] Updated existing Material from disk: " << name << "\n";
				}
				else {
					// V�B�R SPR�VN� PIPELINY PODLE JSONu
					std::shared_ptr<RHIPipeline> selectedPipe = m_pipeInstanced;
					if (pType == "Skinned") selectedPipe = m_pipeSkinned;
					else if (pType == "Solid") selectedPipe = m_pipeSolid;

					auto mat = std::make_shared<Material>(name, selectedPipe, pType);
					mat->Deserialize(j, m_rhi);
					m_allMaterials[name] = mat;
					std::cout << "[AssetRegistry] Loaded new " << pType << " Material from disk: " << name << "\n";
				}
			}
		}
	}
}


void AssetRegistry::SaveAssets() {
	for (const auto& [name, mat] : m_allMaterials) {
		if (name.find("FBX_") != std::string::npos) continue;

		std::string path = "assets/materials/" + name + ".mat";
		std::ofstream file(path);
		if (file.is_open()) {
			file << mat->Serialize().dump(4);
			file.close();
		}
	}
}


std::shared_ptr<SkeletalMesh> AssetRegistry::LoadSkeletalMesh(const std::string& path, const PipelineConfig& cfg) {
	std::string name = fs::path(path).filename().string();
	if (name.empty()) return nullptr;

	if (m_skelMeshes.find(name) != m_skelMeshes.end()) return m_skelMeshes[name];

	if (path.empty() || !fs::exists(path)) {
		std::cerr << "[AssetRegistry] Error: SkeletalMesh file not found at path: " << path << "\n";
		return nullptr;
	}

	std::cout << "[AssetRegistry] Loading new SkeletalMesh: " << path << "\n";
	auto newMesh = std::make_shared<SkeletalMesh>(m_rhi, path, cfg);

	if (!newMesh->scene) return nullptr;

	m_skelMeshes[name] = newMesh;

	for (auto& subMesh : newMesh->subMeshes) {
		if (subMesh.material && m_allMaterials.find(subMesh.material->name) == m_allMaterials.end()) {
			m_allMaterials[subMesh.material->name] = subMesh.material;
		}
	}
	return newMesh;
}


void AssetRegistry::LoadPrimitiveMeshes() {
	if (m_meshes.empty()) {
		m_meshes["Sphere"] = MeshFactory::CreateSphere(m_rhi, 0.5f, 32);
		m_meshes["Cube"] = MeshFactory::CreateCube(m_rhi, 1.0f);
		m_meshes["Capsule"] = MeshFactory::CreateCapsule(m_rhi, 0.5f, 0.5f, 16);
		m_meshes["FloorCube"] = MeshFactory::CreateCube(m_rhi, 1.0f, { 50.0f, 50.0f });
		m_meshes["Plane"] = MeshFactory::CreatePlane(m_rhi, 10.0f, 10.0f, 10, 10, { 1.0f, 1.0f });
		std::cout << "[AssetRegistry] Primitive meshes (Cube, Sphere, Capsule) generated.\n";
	}
}
