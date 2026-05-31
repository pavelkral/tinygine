#include "engine/EngineDependencies.h"
#include "engine/assets/Material.h"

Material::Material(RHI* rhi, const std::string& matName, const std::wstring& vsPath, const std::wstring& psPath, bool useInstancing, bool isSkinned)
	: name(matName) {
	PipelineConfig cfg; cfg.vsPath = vsPath; cfg.psPath = psPath; cfg.useInstancing = useInstancing; cfg.isSkinned = isSkinned;
	cfg.numRenderTargets = 3;
	pipeline = rhi->CreatePipeline(cfg);
	pipelineType = isSkinned ? "Skinned" : (useInstancing ? "Instanced" : "Solid");
}


Material::Material(const std::string& matName, std::shared_ptr<RHIPipeline> sharedPipeline, std::string pType)
	: name(matName), pipeline(sharedPipeline), pipelineType(pType) {
}


void Material::BindTextures(RHI* rhi) {
	if (!pipeline) return;
	rhi->SetPipeline(pipeline.get());
	rhi->SetTexture(albedoTex.get(), 0);
	rhi->SetTexture(normalTex.get(), 1);
	rhi->SetTexture(metalTex.get(), 2);
	rhi->SetTexture(roughTex.get(), 3);
}


json Material::Serialize() {
	json j;
	j["name"] = name;
	j["pipelineType"] = pipelineType; 
	j["baseColor"] = { baseColor.x, baseColor.y, baseColor.z, baseColor.w };
	j["roughness"] = roughness;
	j["metalness"] = metalness;
	j["albedoPath"] = albedoPath;
	j["normalPath"] = normalPath;
	j["metalPath"] = metalPath;
	j["roughPath"] = roughPath;
	return j;
}


void Material::Deserialize(const json& j, RHI* rhi) {
	name = j.value("name", "UnknownMat");
	pipelineType = j.value("pipelineType", "Instanced");

	if (j.contains("baseColor")) baseColor = { j["baseColor"][0], j["baseColor"][1], j["baseColor"][2], j["baseColor"][3] };
	roughness = j.value("roughness", 0.5f);
	metalness = j.value("metalness", 0.0f);
	albedoPath = j.value("albedoPath", "");
	normalPath = j.value("normalPath", "");
	metalPath = j.value("metalPath", "");
	roughPath = j.value("roughPath", "");

	auto loadTex = [&](const std::string& p) -> std::shared_ptr<RHITexture> {
		if (p.empty()) return nullptr;
		std::wstring wp(p.begin(), p.end());
		auto t = rhi->CreateTexture(wp);
		if (!t) t = rhi->CreateDDSTexture(wp);
		return t;
		};

	albedoTex = loadTex(albedoPath);
	normalTex = loadTex(normalPath);
	metalTex = loadTex(metalPath);
	roughTex = loadTex(roughPath);
}
