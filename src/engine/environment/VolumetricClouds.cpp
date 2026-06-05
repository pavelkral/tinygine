#include "engine/environment/VolumetricClouds.h"
#include "stb_image_write.h" 

// =====================================================================
// POMOCNÉ MATEMATICKÉ FUNKCE PRO ŠUM
// =====================================================================
int VolumetricClouds::Wrap(int v, int period) { return (v % period + period) % period; }

float VolumetricClouds::PRandPeriodic(int x, int y, int pX, int pY, int seed) {
    int wx = Wrap(x, pX), wy = Wrap(y, pY);
    float dt = (wx + seed * 131.1f) * 12.9898f + (wy + seed * 17.7f) * 78.233f;
    float s = sinf(fmodf(dt, 3.14159f)) * 43758.5453f;
    return s - floorf(s);
}

float VolumetricClouds::TileableValueNoise(float x, float y, int pW, int pH, int seed) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);

    float bl = PRandPeriodic(ix, iy, pW, pH, seed);
    float br = PRandPeriodic(ix + 1, iy, pW, pH, seed);
    float tl = PRandPeriodic(ix, iy + 1, pW, pH, seed);
    float tr = PRandPeriodic(ix + 1, iy + 1, pW, pH, seed);

    float b = bl + (br - bl) * u;
    float t = tl + (tr - tl) * u;
    return b + (t - b) * v;
}

float VolumetricClouds::TileableFBM(float u, float v, int startPeriod, int octaves, int seed) {
    float val = 0.0f, amp = 0.5f, tot = 0.0f;
    int period = std::max(1, startPeriod);
    for (int i = 0; i < octaves; i++) {
        val += (TileableValueNoise(u * period, v * period, period, period, seed + i * 19) * 2.0f - 1.0f) * amp;
        tot += amp;
        amp *= 0.5f;
        period *= 2;
    }
    return (val / std::max(1e-6f, tot)) * 0.5f + 0.5f;
}

float VolumetricClouds::SmoothStep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}


// =====================================================================
// GENEROVÁNÍ MAPY POÈASÍ
// =====================================================================
void VolumetricClouds::GenerateWeatherMap(RHI* rhi) {
    int w = 512, h = 512;
    std::vector<uint8_t> data(w * h * 4);
    int seed = m_WeatherGen.seed;
    int baseP = std::max(1, m_WeatherGen.basePeriod);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float u = (float)x / (float)w;
            float v = (float)y / (float)h;

            float rawCov = TileableFBM(u, v, baseP, 6, seed);
            float rawType = TileableFBM(u, v, baseP * 2, 4, seed + 100);

            float cov = std::clamp((rawCov - m_WeatherGen.coverageThreshold) * m_WeatherGen.coverageContrast, 0.0f, 1.0f);
            float type = std::clamp((rawType + m_WeatherGen.typeOffset - 0.5f) * m_WeatherGen.typeContrast, 0.0f, 1.0f);

            float stormMask = SmoothStep(0.65f, 0.95f, rawCov) * m_WeatherGen.storminess;
            type = std::lerp(type, 1.0f, stormMask);

            float densVar = TileableFBM(u, v, baseP * 4, 3, seed + 250);
            densVar = std::lerp(0.5f, densVar, m_WeatherGen.densityVar);

            float hVar = TileableFBM(u, v, baseP, 2, seed + 666);
            hVar = std::lerp(0.5f, hVar, m_WeatherGen.heightVar);

            int idx = (y * w + x) * 4;
            data[idx + 0] = (uint8_t)std::clamp(cov * 255.0f, 0.0f, 255.0f);
            data[idx + 1] = (uint8_t)std::clamp(densVar * 255.0f, 0.0f, 255.0f);
            data[idx + 2] = (uint8_t)std::clamp(type * 255.0f, 0.0f, 255.0f);
            data[idx + 3] = (uint8_t)std::clamp(hVar * 255.0f, 0.0f, 255.0f);
        }
    }

    std::string tempPath = "assets/textures/temp_weather.png";
    stbi_write_png(tempPath.c_str(), w, h, 4, data.data(), w * 4);

    std::wstring wPath(tempPath.begin(), tempPath.end());
    m_texWeatherMap = rhi->CreateTexture(wPath);
}


// =====================================================================
// INICIALIZACE A LIFECYCLE
// =====================================================================
bool VolumetricClouds::Init(RHI* rhi) {
    if (!rhi) return false;

    m_texShapeNoise = rhi->CreateUAVTexture3D(128, 128, 128, 1);
    m_texDetailNoise = rhi->CreateUAVTexture3D(64, 64, 64, 1);

    m_csShapeNoise = rhi->CreateComputePipeline(L"shaders/clouds/cloud_shape.comp.hlsl");
    m_csDetailNoise = rhi->CreateComputePipeline(L"shaders/clouds/cloud_detail.comp.hlsl");

    GenerateWeatherMap(rhi);

    PipelineConfig rayCfg;
    rayCfg.vsPath = L"shaders/rhi/fullscreen.vert.hlsl";
    rayCfg.psPath = L"shaders/clouds/cloud_raymarch.frag.hlsl";
    rayCfg.cullMode = CullMode::None;
    rayCfg.depthTest = false;
    rayCfg.depthWrite = false;
    rayCfg.useInstancing = false;
    rayCfg.numRenderTargets = 1;
    m_graphicsRaymarch = rhi->CreatePipeline(rayCfg);

    PipelineConfig upCfg = rayCfg;
    upCfg.psPath = L"shaders/clouds/cloud_upscale.frag.hlsl";
    upCfg.enableBlend = true;
    m_graphicsUpscale = rhi->CreatePipeline(upCfg);

    int w, h; rhi->GetSize(w, h);
    OnResize(rhi, w, h);

    return true;
}

void VolumetricClouds::OnResize(RHI* rhi, int w, int h) {
    m_texHalfRes = rhi->CreateRenderTarget(w / 2, h / 2, 0);
}

void VolumetricClouds::GenerateNoise(RHI* rhi, RHIBuffer* computeUniforms) {
    if (m_isNoiseGenerated) return;

    struct NoiseParams { uint32_t baseFreq; uint32_t seed; uint32_t pad0; uint32_t pad1; };

    if (m_csShapeNoise) {
        NoiseParams sp = { 4, 1337, 0, 0 };
        rhi->SetComputePipeline(m_csShapeNoise.get());
        rhi->SetComputeUniforms(computeUniforms, &sp, sizeof(NoiseParams), 0);
        rhi->SetComputeTextureUAV(m_texShapeNoise.get(), 0);
        rhi->DispatchCompute(128 / 8, 128 / 8, 128 / 8);
        rhi->ComputeBarrier(m_texShapeNoise.get());
    }
    if (m_csDetailNoise) {
        NoiseParams dp = { 4, 4242, 0, 0 };
        rhi->SetComputePipeline(m_csDetailNoise.get());
        rhi->SetComputeUniforms(computeUniforms, &dp, sizeof(NoiseParams), 0);
        rhi->SetComputeTextureUAV(m_texDetailNoise.get(), 0);
        rhi->DispatchCompute(64 / 8, 64 / 8, 64 / 8);
        rhi->ComputeBarrier(m_texDetailNoise.get());
    }
    m_isNoiseGenerated = true;
}


void VolumetricClouds::Render(RHI* rhi, RHIBuffer* computeUniforms, RHIBuffer* globalUniforms, const SM::Matrix& view, const SM::Matrix& proj, double camX, double camY, double camZ, const SM::Vector3& sunDir, float timeSeconds, RHITexture* posTexture, RHITexture* renderTarget) {
    if (!m_texHalfRes) return;
    if (m_weatherMapDirty) {
        GenerateWeatherMap(rhi);
        m_weatherMapDirty = false;
    }

    CloudCB cb = {};
    SM::Matrix invView = view.Invert();
    cb.camRight = { invView._11, invView._12, invView._13, 0.0f };
    cb.camUp = { invView._21, invView._22, invView._23, 0.0f };
    cb.camForward = { invView._31, invView._32, invView._33, 0.0f };

    // PERFEKTNÍ SHODA S HLSL PAMÌTÍ
    cb.camPosAbs = { (float)camX, (float)camY, (float)camZ };
    cb.timeSeconds = timeSeconds * m_Settings.timeScale;
    cb.planetRadius = m_Settings.planetRadius;

    cb.tanHalfFov = 1.0f / proj._22;
    int w, h; rhi->GetSize(w, h);
    cb.aspect = (float)w / (float)h;

    // --- VÝPOÈET OFFSETÙ POMOCÍ DOUBLE (Brání pohybu mrakù s kamerou) ---
    double windDx = (double)cb.timeSeconds * m_Settings.windSpeedX;
    double windDz = (double)cb.timeSeconds * m_Settings.windSpeedZ;

    double wSize = (double)m_Settings.weatherMapSize;
    double wx = fmod(windDx, wSize);
    double wz = fmod(windDz, wSize);
    if (wx < 0) wx += wSize; if (wz < 0) wz += wSize;
    cb.weatherOffset = { (float)wx, (float)wz };

    double sSize = (double)m_Settings.shapeNoiseSize;
    double sx = fmod(windDx, sSize);
    double sz = fmod(windDz, sSize);
    if (sx < 0) sx += sSize; if (sz < 0) sz += sSize;
    cb.shapeOffset = { (float)sx, (float)sz };

    double dSize = (double)m_Settings.detailNoiseSize;
    double dx = fmod(windDx * 1.5, dSize);
    double dz = fmod(windDz * 1.5, dSize);
    if (dx < 0) dx += dSize; if (dz < 0) dz += dSize;
    cb.detailOffset = { (float)dx, (float)dz };
    // ------------------------------------------------------------------

    SM::Vector3 vSun = sunDir; vSun.Normalize();
    cb.sunDir = vSun;

    cb.shapeParams = { m_Settings.coverageBias, m_Settings.densityMult, m_Settings.weatherMapSize, m_Settings.overrideType ? 1.0f : 0.0f };
    cb.typeParams = { m_Settings.typeOverride, m_Settings.erosionStrength, m_Settings.anvilStrength, m_Settings.detailStrength };
    cb.layerParams = { m_Settings.cloudStart, m_Settings.cloudTop, m_Settings.shapeNoiseSize, m_Settings.detailNoiseSize };
    cb.lightParams = { m_Settings.extinction, m_Settings.powderStrength, m_Settings.multiScatter, m_Settings.phaseG };

    cb.cSun = { m_Settings.sunColor[0], m_Settings.sunColor[1], m_Settings.sunColor[2] };
    cb.sunInt = m_Settings.sunIntensity;
    cb.cAmbTop = { m_Settings.ambTop[0], m_Settings.ambTop[1], m_Settings.ambTop[2] };
    cb.ambInt = m_Settings.ambientIntensity;
    cb.cAmbBot = { m_Settings.ambBot[0], m_Settings.ambBot[1], m_Settings.ambBot[2] };
    cb.turbulenceMeters = m_Settings.turbulenceMeters;

    // Tvoje inverzní matice
    cb.invProj = proj.Invert().Transpose();
    cb.horizonFadeEnd = m_Settings.horizonFadeEnd;

    // --- 1. RAYMARCH PASS ---
    std::vector<RHITexture*> halfTargets = { m_texHalfRes.get() };
    rhi->SetMRTTargets(halfTargets, nullptr);
    float clearZero[4] = { 0,0,0,0 };
    rhi->ClearRenderTarget(m_texHalfRes.get(), clearZero);

    rhi->SetPipeline(m_graphicsRaymarch.get());
    rhi->SetGlobalUniforms(globalUniforms, &cb, sizeof(CloudCB));

    rhi->SetTexture(m_texShapeNoise.get(), 0);
    rhi->SetTexture(m_texDetailNoise.get(), 1);
    rhi->SetTexture(posTexture, 2);
    rhi->SetTexture(m_texWeatherMap.get(), 3);

    rhi->Draw(nullptr, 3);

    // --- 2. UPSCALE PASS ---
    if (renderTarget) {
        std::vector<RHITexture*> rts = { renderTarget };
        rhi->SetMRTTargets(rts, nullptr);
    }
    else {
        rhi->SetMainPassTarget();
    }

    rhi->SetPipeline(m_graphicsUpscale.get());
    rhi->SetTexture(m_texHalfRes.get(), 0);
    rhi->SetTexture(posTexture, 1);

    rhi->Draw(nullptr, 3);
}

// =====================================================================
// DEBUG UI
// =====================================================================
void VolumetricClouds::DrawDebug() {
    ImGui::Begin("Cloud Settings");
    ImGui::SetWindowFontScale(1.2f);
    if (ImGui::BeginTabBar("CloudTabs")) {
        if (ImGui::BeginTabItem("Weather Map")) {
            bool regen = false;
            auto markWeatherEdited = [&]() { regen |= ImGui::IsItemDeactivatedAfterEdit(); };
            ImGui::SliderInt("Seed", &m_WeatherGen.seed, 0, 500); markWeatherEdited();
            ImGui::SliderInt("Base Period", &m_WeatherGen.basePeriod, 1, 12); markWeatherEdited();
            ImGui::SliderFloat("Coverage Threshold", &m_WeatherGen.coverageThreshold, 0.0f, 1.0f); markWeatherEdited();
            ImGui::SliderFloat("Coverage Contrast", &m_WeatherGen.coverageContrast, 0.1f, 6.0f); markWeatherEdited();
            ImGui::SliderFloat("Type Offset", &m_WeatherGen.typeOffset, -0.5f, 1.0f); markWeatherEdited();
            ImGui::SliderFloat("Type Contrast", &m_WeatherGen.typeContrast, 0.1f, 6.0f); markWeatherEdited();
            ImGui::SliderFloat("Storminess", &m_WeatherGen.storminess, 0.0f, 1.0f); markWeatherEdited();
            ImGui::SliderFloat("Density Var", &m_WeatherGen.densityVar, 0.0f, 1.0f); markWeatherEdited();
            ImGui::SliderFloat("Height Var", &m_WeatherGen.heightVar, 0.0f, 1.0f); markWeatherEdited();
            regen |= ImGui::Button("Regenerate Weather Map");
            if (regen) m_weatherMapDirty = true;
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Clouds")) {
            ImGui::SliderFloat("Coverage Bias", &m_Settings.coverageBias, 0.0f, 1.0f);
            ImGui::SliderFloat("Density Mult", &m_Settings.densityMult, 0.1f, 10.0f);
            ImGui::SliderFloat("Cloud Start (m)", &m_Settings.cloudStart, 200.0f, 5000.0f);
            ImGui::SliderFloat("Cloud Top (m)", &m_Settings.cloudTop, 2000.0f, 14000.0f);
            ImGui::SliderFloat("Planet Radius", &m_Settings.planetRadius, 100000.0f, 10000000.0f);

            ImGui::Separator();
            ImGui::Text("Physical Scale (Meters)");
            ImGui::SliderFloat("Weather Map Size", &m_Settings.weatherMapSize, 100000.0f, 2000000.0f, "%.0f m");
            ImGui::SliderFloat("Shape Noise Size", &m_Settings.shapeNoiseSize, 1000.0f, 50000.0f, "%.0f m");
            ImGui::SliderFloat("Detail Noise Size", &m_Settings.detailNoiseSize, 100.0f, 5000.0f, "%.0f m");

            ImGui::Separator();
            ImGui::SliderFloat("Erosion", &m_Settings.erosionStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Detail Strength", &m_Settings.detailStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Turbulence (m)", &m_Settings.turbulenceMeters, 0.0f, 300.0f);
            ImGui::SliderFloat("Anvil Strength", &m_Settings.anvilStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Horizon Fade", &m_Settings.horizonFadeEnd, 0.002f, 0.080f, "%.4f");

            ImGui::Separator();
            ImGui::SliderFloat("Extinction", &m_Settings.extinction, 0.0003f, 0.0040f, "%.6f");
            ImGui::SliderFloat("Powder", &m_Settings.powderStrength, 0.0f, 4.0f);
            ImGui::SliderFloat("MultiScatter", &m_Settings.multiScatter, 0.0f, 2.0f);
            ImGui::SliderFloat("PhaseG", &m_Settings.phaseG, 0.0f, 0.95f);
            ImGui::SliderFloat("Sun Intensity", &m_Settings.sunIntensity, 0.0f, 10.0f);
            ImGui::SliderFloat("Ambient Intensity", &m_Settings.ambientIntensity, 0.0f, 3.0f);
            ImGui::ColorEdit3("Sun Color", m_Settings.sunColor);
            ImGui::ColorEdit3("Amb Top", m_Settings.ambTop);
            ImGui::ColorEdit3("Amb Bot", m_Settings.ambBot);

            ImGui::Separator();
            ImGui::SliderFloat("Wind Speed X", &m_Settings.windSpeedX, -50.0f, 550.0f, "%.1f m/s");
            ImGui::SliderFloat("Wind Speed Z", &m_Settings.windSpeedZ, -50.0f, 550.0f, "%.1f m/s");
            ImGui::SliderFloat("Time Scale", &m_Settings.timeScale, 0.0f, 100.0f, "%.1fx");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}