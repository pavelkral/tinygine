#pragma once


#include "pch/Pch.h"
#include "engine/EngineDependencies.h"

struct AtmosphereCBuffer {
	XMFLOAT3 sunDir;
	float planetRadius;
	XMFLOAT3 rayleighCoeff;
	float atmosphereRadius;
	XMFLOAT3 mieCoeff;
	float rayleighScaleHeight;
	XMFLOAT3 ozoneCoeff;
	float mieScaleHeight;
	XMFLOAT3 cameraPos;
	float mieG;
	float sunIntensity;
	float padding[3];
};

class Atmosphere {
private:
	std::shared_ptr<RHIPipeline> m_csTransmittance;
	std::shared_ptr<RHIPipeline> m_csSkyView;
	std::shared_ptr<RHIPipeline> m_graphicsSky;

	std::shared_ptr<RHITexture> m_texTransmittance;
	std::shared_ptr<RHITexture> m_texSkyView;

	std::shared_ptr<Mesh> m_skySphere;

public:
	float m_PlanetRadius = 6360000.0f;
	float m_AtmosphereThickness = 100000.0f;
	float m_SunIntensity = 15.0f;
	float m_MieG = 0.8f;
	float m_RayleighScale = 1.0f;
	bool Init(RHI* rhi);
	void ComputeLUTs(RHI* rhi, RHIBuffer* computeUniforms,
		const SM::Vector3& sunDir, const SM::Vector3& camPos);
	void RenderSky(RHI* rhi, RHIBuffer* globalBuffer, const GlobalData& gData);
	void DrawDebug();
};


