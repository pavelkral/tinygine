#pragma once

#include "pch/Pch.h"
#include "engine/EngineDependencies.h"

// Pøesnì 512 bytù pro DX12 RingBuffer
struct CloudCB {
    SM::Vector4 camForward;
    SM::Vector4 camRight;
    SM::Vector4 camUp;
    SM::Vector3 camPosAbs; float timeSeconds;
    SM::Vector3 sunDir;    float planetRadius;

    // Precizní CPU offsety pro fixaci mrakù na svìtových souøadnicích
    SM::Vector2 weatherOffset;
    SM::Vector2 shapeOffset;
    SM::Vector2 detailOffset;
    float tanHalfFov;
    float aspect;

    SM::Vector4 shapeParams;
    SM::Vector4 typeParams;
    SM::Vector4 layerParams;
    SM::Vector4 lightParams;

    SM::Vector3 cSun;    float sunInt;
    SM::Vector3 cAmbTop; float ambInt;
    SM::Vector3 cAmbBot; float pad1;

    SM::Matrix invProj;

    // Pøesnì 76 floatù (304 bytù) zarovná celou strukturu na dokonalých 512 bytù
    float padding[76];
};

class VolumetricClouds {
public:
    struct Settings {
        float coverageBias = 0.05f;
        float densityMult = 4.5f;
        float cloudStart = 1700.0f;
        float cloudTop = 10000.0f;
        float planetRadius = 6371000.0f;

        float weatherMapSize = 1000000.0f;
        float shapeNoiseSize = 35000.0f;
        float detailNoiseSize = 3500.0f;

        float erosionStrength = 0.50f;
        float detailStrength = 0.80f;
        float turbulenceMeters = 150.0f;
        float anvilStrength = 1.0f;

        float extinction = 0.0010f;
        float powderStrength = 2.0f;
        float multiScatter = 0.85f;
        float phaseG = 0.65f;

        float sunIntensity = 5.0f;
        float ambientIntensity = 0.85f;

        float sunColor[3] = { 1.0f, 0.95f, 0.85f };
        float ambTop[3] = { 0.65f, 0.75f, 0.90f };
        float ambBot[3] = { 0.12f, 0.18f, 0.28f };

        float timeScale = 1.0f;
        float windSpeedX = 15.0f;
        float windSpeedZ = 5.0f;

        bool  overrideType = false;
        float typeOverride = 0.65f;
    } m_Settings;

    struct WeatherGenSettings {
        int seed = 256;
        int basePeriod = 6;
        float coverageThreshold = 0.35f;
        float coverageContrast = 2.5f;
        float typeOffset = 0.40f;
        float typeContrast = 1.5f;
        float storminess = 0.65f;
        float densityVar = 0.75f;
        float heightVar = 0.50f;
    } m_WeatherGen;

    bool Init(RHI* rhi);
    void Render(RHI* rhi, RHIBuffer* computeUniforms, RHIBuffer* globalUniforms, const SM::Matrix& view, const SM::Matrix& proj, const SM::Vector3& camPosWorld, const SM::Vector3& sunDir, float timeSeconds, RHITexture* posTexture, RHITexture* renderTarget);
    void DrawDebug();

private:
    std::shared_ptr<RHIPipeline> m_csShapeNoise;
    std::shared_ptr<RHIPipeline> m_csDetailNoise;
    std::shared_ptr<RHIPipeline> m_graphicsRaymarch;
    std::shared_ptr<RHIPipeline> m_graphicsUpscale;

    std::shared_ptr<RHITexture> m_texShapeNoise;
    std::shared_ptr<RHITexture> m_texDetailNoise;
    std::shared_ptr<RHITexture> m_texWeatherMap;
    std::shared_ptr<RHITexture> m_texHalfRes;

    bool m_isNoiseGenerated = false;

    void GenerateWeatherMap(RHI* rhi);
    int Wrap(int v, int period);
    float PRandPeriodic(int x, int y, int pX, int pY, int seed);
    float TileableValueNoise(float x, float y, int pW, int pH, int seed);
    float TileableFBM(float u, float v, int startPeriod, int octaves, int seed);
    float SmoothStep(float edge0, float edge1, float x);
};