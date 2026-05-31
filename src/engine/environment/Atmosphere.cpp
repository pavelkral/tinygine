#include "engine/environment/Atmosphere.h"
#include "engine/EngineDependencies.h"

bool Atmosphere::Init(RHI* rhi) {
	if (!rhi)
		return false;

	m_texTransmittance = rhi->CreateUAVTexture(256, 64, 1);
	m_texSkyView = rhi->CreateUAVTexture(256, 128, 1);

	m_csTransmittance =
		rhi->CreateComputePipeline(L"shaders/rhi/atm_trans.comp.hlsl");
	m_csSkyView =
		rhi->CreateComputePipeline(L"shaders/rhi/atm_skyview.comp.hlsl");

	PipelineConfig skyCfg;
	skyCfg.vsPath = L"shaders/rhi/atm_sky.vert.hlsl";
	skyCfg.psPath = L"shaders/rhi/atm_sky.frag.hlsl";
	skyCfg.cullMode = CullMode::None;
	skyCfg.depthTest = true;
	skyCfg.depthWrite = false;
	skyCfg.useInstancing = false;
	skyCfg.isSkinned = false;
	skyCfg.numRenderTargets = 3;
	m_graphicsSky = rhi->CreatePipeline(skyCfg);

	m_skySphere = MeshFactory::CreateSphere(rhi, 100.0f, 32);

	return true;
}

void Atmosphere::ComputeLUTs(RHI* rhi, RHIBuffer* computeUniforms,
	const SM::Vector3& sunDir,
	const SM::Vector3& camPos) {
	AtmosphereCBuffer params = {};
	params.sunDir = { sunDir.x, sunDir.y, sunDir.z };
	params.planetRadius = m_PlanetRadius;
	params.atmosphereRadius = m_PlanetRadius + m_AtmosphereThickness;
	params.rayleighCoeff = { 0.0000058f * m_RayleighScale,
							0.0000135f * m_RayleighScale,
							0.0000331f * m_RayleighScale };
	params.mieCoeff = { 0.0000039f, 0.0000039f, 0.0000039f };
	params.ozoneCoeff = { 0.0000006f, 0.0000021f, 0.0000000f };
	params.rayleighScaleHeight = 8000.0f;
	params.mieScaleHeight = 1200.0f;
	params.mieG = m_MieG;
	params.sunIntensity = m_SunIntensity;
	params.cameraPos = { 0.0f, m_PlanetRadius + std::max((float)camPos.y, 10.0f),
						0.0f };

	rhi->SetComputePipeline(m_csTransmittance.get());
	rhi->SetComputeUniforms(computeUniforms, &params, sizeof(AtmosphereCBuffer),
		0);
	rhi->SetComputeTextureUAV(m_texTransmittance.get(), 0);
	rhi->DispatchCompute(256 / 8, 64 / 8, 1);
	rhi->ComputeBarrier(m_texTransmittance.get());

	rhi->SetComputePipeline(m_csSkyView.get());
	rhi->SetComputeUniforms(computeUniforms, &params, sizeof(AtmosphereCBuffer),
		0);
	rhi->SetComputeTextureUAV(m_texSkyView.get(), 0);
	rhi->SetComputeTextureSRV(m_texTransmittance.get(), 0);
	rhi->DispatchCompute(256 / 8, 128 / 8, 1);
	rhi->ComputeBarrier(m_texSkyView.get());
}

void Atmosphere::RenderSky(RHI* rhi, RHIBuffer* globalBuffer,
	const GlobalData& gData) {
	rhi->SetPipeline(m_graphicsSky.get());
	rhi->SetGlobalUniforms(globalBuffer, &gData, sizeof(GlobalData));

	for (int i = 0; i < 8; i++)
		rhi->SetTexture(nullptr, i);
	rhi->SetTexture(m_texSkyView.get(), 0);

	rhi->DrawIndexed(m_skySphere->vb.get(), m_skySphere->ib.get(),
		m_skySphere->indexCount);
}

void Atmosphere::DrawDebug() {
	ImGui::Begin("Atmosphere Settings");
	if (ImGui::CollapsingHeader("Atmosphere ", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Planet Radius (m)", &m_PlanetRadius, 1000000.0f,
			10000000.0f);
		ImGui::SliderFloat("Atmosphere Height", &m_AtmosphereThickness, 10000.0f,
			200000.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Rayleigh Strength", &m_RayleighScale, 0.1f, 10.0f);
		ImGui::SliderFloat("Mie G (Sun Halo)", &m_MieG, 0.0f, 0.999f);
		ImGui::SliderFloat("Sun Intensity", &m_SunIntensity, 1.0f, 50.0f);
	}
	ImGui::End();
}
