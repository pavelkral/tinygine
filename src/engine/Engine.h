#pragma once

#include "engine/EngineDependencies.h"
#include "engine/environment/VolumetricClouds.h"
#include "engine/terrain/TerrainManager.h"

/// ///////////////////////////////////////////////////////////////////////////
/// --- ENGINE class ---
/// ///////////////////////////////////////////////////////////////////////////

class Engine {

private:
    HWND m_hwnd;
    int m_apiChoice;
    std::unique_ptr<RHI> m_rhi;

    EngineConfig m_config;
    AssetRegistry m_assets;
    SceneManager m_sceneManager;

    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<BPLayerInterfaceImpl> m_bpLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objVsBpFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objPairFilter;
    std::unique_ptr<JPH::PhysicsSystem> m_physics;
    std::unique_ptr<JoltDebugRenderer> m_debugRenderer;
    std::unique_ptr<MyContactListener> m_contactListener;
    bool m_debugPhysics = false;

    std::shared_ptr<RHIPipeline> m_linePipeline;
    std::shared_ptr<RHIBuffer> m_lineBuffer;
    std::vector<Vertex> m_lineVertices;

    std::shared_ptr<RHITexture> m_shadowMap;
    std::shared_ptr<RHIPipeline> m_shadowPipeline;
    std::shared_ptr<RHIPipeline> m_skinnedShadowPipeline;

    std::shared_ptr<RHIBuffer> m_globalBuffer;
    std::shared_ptr<RHIBuffer> m_instanceBuffer;
    std::shared_ptr<RHIBuffer> m_computeUniformBuffer;
    std::shared_ptr<RHIBuffer> m_skinnedObjectBuffer;
    std::shared_ptr<RHIBuffer> m_boneBuffer;

    std::shared_ptr<RHITexture> m_irradianceMap;
    std::shared_ptr<RHITexture> m_prefilterMap;
    std::shared_ptr<RHITexture> m_brdfLut;

    std::shared_ptr<RHITexture> m_rtColor;
    std::shared_ptr<RHITexture> m_rtNormal;
    std::shared_ptr<RHITexture> m_rtPos;
    std::shared_ptr<RHITexture> m_rtSceneFinal;
    std::shared_ptr<RHITexture> m_rtPingPong;

    std::shared_ptr<RHITexture> m_rtSSAO;
    std::shared_ptr<RHITexture> m_rtSSAOBlur;
    std::shared_ptr<RHIPipeline> m_ssaoPipeline;
    std::shared_ptr<RHIPipeline> m_ssaoBlurPipeline;
    std::shared_ptr<RHIPipeline> m_ssaoCombinePipeline;
    SSAOParams m_ssaoParams;
    bool m_enableSSAO = true;

    std::shared_ptr<RHIPipeline> m_ssrPipeline;
    std::shared_ptr<RHIPipeline> m_bloomPipeline;
    std::shared_ptr<RHIPipeline> m_vignettePipeline;
    std::shared_ptr<RHIPipeline> m_copyPipeline;

    ma_engine m_audioEngine;

    bool m_enableSSR = true;
    bool m_enableBloom = false;
    bool m_enableVignette = true;

    float m_accumulator = 0.0f;
    std::chrono::high_resolution_clock::time_point m_lastTime;

    SimState m_simState = SimState::Stopped;
    bool m_cameraActive = false;
    bool m_enablePhysicallyBasedSky = true;
    bool m_enableClouds = false;

    std::unique_ptr<Skybox> m_skybox;
    std::unique_ptr<Atmosphere> m_atmosphere;
    std::unique_ptr<VolumetricClouds> m_Clouds;

    // --- TERRAIN ---
    TerrainManager m_terrainManager;
    int m_iCurrentZoom = 13;
    int m_iLastZoom = -1;
    bool m_bAutoTerrainZoom = true;
    float m_fTerrainExaggeration = 1.0f;
    float m_fCurrentGroundHeight = 0.0f;
    int m_iVisibleTileX = 0;
    int m_iVisibleTileY = 0;
    FpsCamera m_camera;

    ImGui::FileBrowser m_loadDialog;
    ImGui::FileBrowser m_saveDialog;

    fs::path m_currentAssetDirectory = "assets";
    void RecreateRenderTargets();
    void InitSSAO();

public:
    Engine();
    ~Engine();
    bool OnInit(HINSTANCE hInstance, int nCmdShow);
    void Run();
    int CalculateBestZoom(double fAltitude);
    void CheckCollisions();

public:
private:
    void LoadResourcesAndScene();
    void BuildHardcodedScene();
    void OnInput(float dt);
    void OnPhysicsUpdate(float dt);
    void OnUpdate(float dt);
    void OnRender();
    void SpawnModelFromAsset(const std::string &path, float mouseX = -1.0f,
                             float mouseY = -1.0f, float screenW = 1.0f,
                             float screenH = 1.0f);
    void RenderAssetBrowser();
    void RenderEditorUI(const ImGuiViewport *vp_imgui, float screenW,
                        float screenH, XMMATRIX proj, XMMATRIX view);
    void OnQuit();
};
