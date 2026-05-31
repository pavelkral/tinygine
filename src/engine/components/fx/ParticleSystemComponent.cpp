#include "engine/EngineDependencies.h"
#include "engine/components/fx/ParticleSystemComponent.h"

std::shared_ptr<Mesh> ParticleSystemComponent::CreateQuad(RHI* rhi) {
	std::vector<Vertex> v = {
		{{-0.5f, -0.5f, 0.0f}, {0,0,-1}, {0,1}},
		{{-0.5f,  0.5f, 0.0f}, {0,0,-1}, {0,0}},
		{{ 0.5f,  0.5f, 0.0f}, {0,0,-1}, {1,0}},
		{{ 0.5f, -0.5f, 0.0f}, {0,0,-1}, {1,1}}
	};
	std::vector<uint32_t> idx = { 0, 1, 2, 0, 2, 3 };
	return std::make_shared<Mesh>(rhi, v, idx);
}


ParticleSystemComponent::ParticleSystemComponent(RHI* rhi, const std::wstring& texturePath, int maxParticles)
	: m_rhi(rhi), m_maxParticles(maxParticles) {
	componentType = "ParticleSystemComponent";
	if (!rhi) return;

	m_computePipeline = rhi->CreateComputePipeline(L"shaders/rhi/particle.comp.hlsl");

	PipelineConfig cfg;
	cfg.vsPath = L"shaders/rhi/particle.vert.hlsl";
	cfg.psPath = L"shaders/rhi/particle.frag.hlsl";
	cfg.isTransparent = true;
	cfg.isAdditive = false;
	cfg.depthTest = true;
	cfg.depthWrite = false;
	cfg.cullMode = CullMode::None;
	cfg.useInstancing = true;
	cfg.numRenderTargets = 3;
	m_renderPipeline = rhi->CreatePipeline(cfg);

	std::vector<InitialParticle> initialData(m_maxParticles);
	for (int i = 0; i < m_maxParticles; i++) {
		initialData[i].life = -((rand() % 400) / 100.0f);
	}

	m_particleBuffer = rhi->CreateBuffer(BufferType::ComputeUAV, initialData.data(), initialData.size() * sizeof(InitialParticle), 112);
	m_quadMesh = CreateQuad(rhi);
	if (!texturePath.empty()) m_texture = rhi->CreateTexture(texturePath);
}


json ParticleSystemComponent::Serialize() {
	json j = Component::Serialize();
	j["localDirection"] = { localDirection.x, localDirection.y, localDirection.z };
	j["isPlaying"] = isPlaying;
	return j;
}


void ParticleSystemComponent::Deserialize(const json& j) {
	if (j.contains("localDirection")) {
		localDirection = { j["localDirection"][0], j["localDirection"][1], j["localDirection"][2] };
	}
	isPlaying = j.value("isPlaying", true);
}


void ParticleSystemComponent::Play() {
	if (!isPlaying) {
		isPlaying = true;
		m_totalTime = 0.0f;
	}
}


void ParticleSystemComponent::Stop() {
	isPlaying = false;
}


void ParticleSystemComponent::Update(float dt) {
	m_pendingDt = dt;
}


void ParticleSystemComponent::DispatchGPU(RHIBuffer* computeUniforms) {
	if (!m_rhi || !m_computePipeline || !isPlaying || m_pendingDt == 0.0f) return;

	m_totalTime += m_pendingDt;

	ParticleComputeParams params = {};
	params.dt = m_pendingDt;
	params.totalTime = m_totalTime;

	SM::Vector3 p((float)gameObject->transform.position.x, (float)gameObject->transform.position.y, (float)gameObject->transform.position.z);
	params.emitterPos = p;
	params.emitDir = SM::Vector3::Transform(localDirection, SM::Matrix::CreateFromQuaternion(gameObject->transform.rotation));

	m_rhi->SetComputePipeline(m_computePipeline.get());
	m_rhi->SetComputeUniforms(computeUniforms, &params, sizeof(ParticleComputeParams), 0);
	m_rhi->SetComputeBufferUAV(m_particleBuffer.get(), 0);

	m_rhi->DispatchCompute(m_maxParticles / 64 + 1, 1, 1);
	m_rhi->ComputeBufferBarrier(m_particleBuffer.get());
}


void ParticleSystemComponent::Render(RHIBuffer* globalBuffer, const GlobalData& gData) {
	if (!m_rhi || !m_renderPipeline || !isPlaying) return;

	m_rhi->SetPipeline(m_renderPipeline.get());
	m_rhi->SetGlobalUniforms(globalBuffer, &gData, sizeof(GlobalData));

	for (int i = 0; i < 8; i++) m_rhi->SetTexture(nullptr, i);
	if (m_texture) m_rhi->SetTexture(m_texture.get(), 0);

	m_rhi->DrawIndexedInstanced(m_quadMesh->vb.get(), m_quadMesh->ib.get(), m_particleBuffer.get(), m_quadMesh->indexCount, m_maxParticles, 0);
}


void ParticleSystemComponent::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Particle System");
	ImGui::SameLine();
	if (ImGui::Button("Remove##part")) Destroy();

	ImGui::Text("Status: %s", isPlaying ? "PLAYING" : "STOPPED");
	if (ImGui::Button("Play", ImVec2(80, 0))) Play();
	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(80, 0))) Stop();
	ImGui::DragFloat3("Local Direction", &localDirection.x, 0.05f, -1.0f, 1.0f);
}
