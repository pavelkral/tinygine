#pragma once

#include "pch/Pch.h"

struct InitialParticle {
	float pos[3]; float life;
	float vel[3]; float size;
	float color[4];
	float pad1[4]; float pad2[4]; float pad3[4]; float pad4[4];
};

struct ParticleComputeParams {
	float dt;
	float totalTime;
	float pad[2];
	SM::Vector3 emitterPos;
	float pad2;
	SM::Vector3 emitDir;
	float pad3;
};

class ParticleSystemComponent : public Component {
private:
	RHI* m_rhi = nullptr;
	std::shared_ptr<RHIPipeline> m_computePipeline;
	std::shared_ptr<RHIPipeline> m_renderPipeline;
	std::shared_ptr<RHIBuffer> m_particleBuffer;
	std::shared_ptr<Mesh> m_quadMesh;
	std::shared_ptr<RHITexture> m_texture;

	int m_maxParticles = 10000;
	float m_totalTime = 0.0f;
	float m_pendingDt = 0.0f;
	std::shared_ptr<Mesh> CreateQuad(RHI* rhi);

public:
	SM::Vector3 localDirection = { 0.0f, 1.0f, 0.0f };
	bool isPlaying = true;
	ParticleSystemComponent(RHI* rhi = nullptr,
		const std::wstring& texturePath = L"",
		int maxParticles = 500);
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Play();
	void Stop();
	void Update(float dt) override;
	void DispatchGPU(RHIBuffer* computeUniforms);
	void Render(RHIBuffer* globalBuffer, const GlobalData& gData);
	void OnGUI() override;
};

