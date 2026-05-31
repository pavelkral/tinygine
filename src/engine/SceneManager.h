#pragma once

#include "pch/Pch.h"
#include "engine/scene/GameObject.h"

class RHI;
class AssetRegistry;
class FpsCamera;

struct GraveyardItem {
    std::unique_ptr<GameObject> obj;
    int framesToLive;
};

class SceneManager {
public:
    std::vector<std::unique_ptr<GameObject>> m_objects;
    std::vector<std::unique_ptr<GameObject>> m_pendingObjects;
    std::vector<GraveyardItem> m_graveyard;
    GameObject* m_selectedObject = nullptr;
    std::string m_currentScenePath = "";

    RHI* m_rhi = nullptr;
    JPH::PhysicsSystem* m_physics = nullptr;
    AssetRegistry* m_assets = nullptr;
    ma_engine* m_audioEngine = nullptr;
    FpsCamera* m_camera = nullptr;    
    void Init(RHI* rhi, JPH::PhysicsSystem* physics, AssetRegistry* assets, ma_engine* audio, FpsCamera* cam);    
    void AddObject(std::unique_ptr<GameObject> obj);    
    void ClearScene();    
    void ResetScene();    
    void StartAll();    
    void Update(float dt);    
    void FixedUpdate(float fixedDt);  
    void Cleanup();    
    void SaveScene(const std::string& filepath);    
    void LoadScene(const std::string& filepath);    
};
