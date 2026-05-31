#include "engine/SceneManager.h"
#include "engine/EngineDependencies.h"

void SceneManager::Init(RHI *rhi, JPH::PhysicsSystem *physics,
                        AssetRegistry *assets, ma_engine *audio,
                        FpsCamera *cam) {
    m_rhi = rhi;
    m_physics = physics;
    m_assets = assets;
    m_audioEngine = audio;
    m_camera = cam;
}

void SceneManager::AddObject(std::unique_ptr<GameObject> obj) {
    m_pendingObjects.push_back(std::move(obj));
}

void SceneManager::ClearScene() {
    m_selectedObject = nullptr;
    for (auto &obj : m_objects)
        obj->Destroy();
    m_currentScenePath = "";
}

void SceneManager::ResetScene() {
    for (auto &obj : m_objects)
        obj->Reset();
}

void SceneManager::StartAll() {
    for (auto &o : m_objects)
        o->Start();
    for (auto &o : m_pendingObjects)
        o->Start();
    if (m_physics)
        m_physics->OptimizeBroadPhase();
}

void SceneManager::Update(float dt) {
    for (auto &obj : m_objects)
        if (!obj->isPendingDestroy)
            obj->Update(dt);
}

void SceneManager::FixedUpdate(float fixedDt) {
    for (auto &obj : m_objects)
        if (!obj->isPendingDestroy)
            obj->FixedUpdate(fixedDt);
}

void SceneManager::Cleanup() {
    for (auto it = m_objects.begin(); it != m_objects.end();) {
        if ((*it)->isPendingDestroy) {
            if (m_selectedObject == it->get())
                m_selectedObject = nullptr;
            m_graveyard.push_back({std::move(*it), 3});
            it = m_objects.erase(it);
        } else {
            ++it;
        }
    }
    for (auto &item : m_graveyard)
        item.framesToLive--;
    m_graveyard.erase(std::remove_if(m_graveyard.begin(), m_graveyard.end(),
                                     [](const GraveyardItem &item) {
                                         return item.framesToLive <= 0;
                      }),
                      m_graveyard.end());
    for (auto &newObj : m_pendingObjects)
        m_objects.push_back(std::move(newObj));
    m_pendingObjects.clear();
}

void SceneManager::SaveScene(const std::string &filepath) {
    json jScene = json::array();
    for (auto &obj : m_objects) {
        if (!obj->isPendingDestroy)
            jScene.push_back(obj->Serialize());
    }
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << jScene.dump(4);
        file.close();
        std::cout << "[SceneManager] Scene saved to: " << filepath << "\n";
    }

    if (m_assets)
        m_assets->SaveAssets();
}

void SceneManager::LoadScene(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open())
        return;
    json jScene;
    file >> jScene;
    file.close();

    ClearScene();
    JPH::BodyInterface &bi = m_physics->GetBodyInterface();

    for (const auto &jObj : jScene) {
        auto go = std::make_unique<GameObject>("");
        go->DeserializeBase(jObj);

        if (jObj.contains("components")) {
            for (const auto &jComp : jObj["components"]) {
                std::string type = jComp.value("type", "");

                if (type == "DirectionalLight")
                    go->AddComponent<DirectionalLight>()->Deserialize(jComp);
                else if (type == "PointLight")
                    go->AddComponent<PointLight>()->Deserialize(jComp);
                else if (type == "BoxCollider")
                    go->AddComponent<BoxCollider>(SM::Vector3(1, 1, 1))
                        ->Deserialize(jComp);
                else if (type == "SphereCollider")
                    go->AddComponent<SphereCollider>(0.5f)->Deserialize(jComp);
                else if (type == "CapsuleCollider")
                    go->AddComponent<CapsuleCollider>(0.5f, 0.5f)->Deserialize(jComp);
                else if (type == "MeshCollider") {
                    auto mc = go->AddComponent<MeshCollider>();
                    mc->Deserialize(jComp);
                    if (m_assets->m_meshes.count(mc->meshName)) {
                        mc->collisionMesh = m_assets->m_meshes[mc->meshName];
                    } else if (m_assets->m_skelMeshes.count(mc->meshName)) {
                        mc->collisionSkelMesh = m_assets->m_skelMeshes[mc->meshName];
                    }
                } else if (type == "Rigidbody") {
                    bool isDyn = jComp.value("isDynamic", false);
                    go->AddComponent<Rigidbody>(&bi, isDyn)->Deserialize(jComp);
                } else if (type == "MeshRenderer") {
                    auto mr = go->AddComponent<MeshRenderer>();
                    mr->Deserialize(jComp);
                    if (m_assets->m_meshes.count(mr->meshName))
                        mr->mesh = m_assets->m_meshes[mr->meshName];
                    if (m_assets->m_allMaterials.count(mr->matName))
                        mr->material = m_assets->m_allMaterials[mr->matName];
                } else if (type == "SkinnedMeshRenderer") {
                    auto smr = go->AddComponent<SkinnedMeshRenderer>();
                    smr->Deserialize(jComp);
                    if (!smr->meshName.empty() && !smr->meshPath.empty()) {
                        PipelineConfig skelCfg;
                        skelCfg.vsPath = L"shaders/rhi/pbr-skinned.vert.hlsl";
                        skelCfg.psPath = L"shaders/rhi/pbr-skinned.frag.hlsl";
                        skelCfg.useInstancing = false;
                        skelCfg.isSkinned = true;
                        skelCfg.numRenderTargets = 3;
                        smr->mesh = m_assets->LoadSkeletalMesh(smr->meshPath, skelCfg);
                        if (smr->mesh) {
                            smr->materialOverrides.resize(smr->mesh->subMeshes.size(),
                                                          nullptr);
                            for (size_t i = 0; i < smr->overrideMatNames.size(); ++i) {
                                if (i < smr->materialOverrides.size() &&
                                    !smr->overrideMatNames[i].empty() &&
                                    m_assets->m_allMaterials.count(smr->overrideMatNames[i])) {
                                    smr->SetMaterial(
                                        i, m_assets->m_allMaterials[smr->overrideMatNames[i]]);
                                }
                            }
                        }
                    }
                } else if (type == "Animator") {
                    auto anim = go->AddComponent<Animator>();
                    anim->Deserialize(jComp);
                    if (auto smr = go->GetComponent<SkinnedMeshRenderer>())
                        anim->skelMesh = smr->mesh;
                } else if (type == "AudioSource")
                    go->AddComponent<AudioSource>(m_audioEngine, "")->Deserialize(jComp);
                else if (type == "ParticleSystemComponent")
                    go->AddComponent<ParticleSystemComponent>(m_rhi, L"", 500)
                        ->Deserialize(jComp);
                else if (type == "PlayerController")
                    go->AddComponent<PlayerController>()->Deserialize(jComp);
                else if (type == "RotatingObstacle")
                    go->AddComponent<RotatingObstacle>()->Deserialize(jComp);
                else if (type == "BouncingJumper")
                    go->AddComponent<BouncingJumper>()->Deserialize(jComp);
                else if (type == "BulletLife")
                    go->AddComponent<BulletLife>()->Deserialize(jComp);
                else if (type == "PlayerJumper")
                    go->AddComponent<PlayerJumper>()->Deserialize(jComp);
                else if (type == "SkeletalRagdollComponent") {
                    auto rag = go->AddComponent<SkeletalRagdollComponent>();
                    rag->Deserialize(jComp);
                    rag->m_physics = m_physics;
                }

                else if (type == "PlayerShooter") {
                    auto ps = go->AddComponent<PlayerShooter>(m_camera, &bi, nullptr,
                                                              nullptr, nullptr);
                    ps->Deserialize(jComp);
                } else if (type == "Ball")
                    go->AddComponent<Ball>()->Deserialize(jComp);
                else if (type == "Brick")
                    go->AddComponent<Brick>(m_audioEngine)->Deserialize(jComp);
                else if (type == "Paddle")
                    go->AddComponent<Paddle>()->Deserialize(jComp);
                else if (type == "DeathZone")
                    go->AddComponent<DeathZone>()->Deserialize(jComp);
            }
        }
        AddObject(std::move(go));
    }

    Paddle *loadedPaddle = nullptr;
    for (auto &obj : m_pendingObjects)
        if (auto p = obj->GetComponent<Paddle>())
            loadedPaddle = p;
    for (auto &obj : m_pendingObjects)
        if (auto dz = obj->GetComponent<DeathZone>())
            dz->paddleRef = loadedPaddle;

    for (auto &obj : m_pendingObjects) {
        if (auto pad = obj->GetComponent<Paddle>()) {
            auto meshSphere = m_assets->m_meshes.count("Sphere")
                                  ? m_assets->m_meshes["Sphere"]
                                  : nullptr;
            auto matBall = m_assets->m_allMaterials.count("Mat_Ball")
                               ? m_assets->m_allMaterials["Mat_Ball"]
                               : nullptr;

            pad->spawnBallCallback = [this, meshSphere, matBall, &bi](float px,
                                                                      float py) {
                auto ball = std::make_unique<GameObject>("Ball");
                ball->transform.position = {px, py, 0.0f};
                ball->transform.scale = {1.0f, 1.0f, 1.0f};
                ball->AddComponent<MeshRenderer>(meshSphere, matBall, true, "Sphere");
                ball->AddComponent<SphereCollider>(0.5f);
                auto rb = ball->AddComponent<Rigidbody>(&bi, true);
                rb->friction = 0.0f;
                rb->restitution = 1.0f;
                ball->AddComponent<Ball>();
                ball->AddComponent<ParticleSystemComponent>(m_rhi, L"");
                auto light = ball->AddComponent<PointLight>();
                light->color = {1.0f, 1.0f, 1.0f};
                light->intensity = 20.0f;
                light->radius = 10.0f;
                ball->Start();
                this->AddObject(std::move(ball));
            };
        }
        if (auto shooter = obj->GetComponent<PlayerShooter>()) {
            shooter->m_camera = m_camera;
            shooter->m_physics = &bi;
            shooter->m_bulletMesh = m_assets->m_meshes.count("Sphere")
                                        ? m_assets->m_meshes["Sphere"]
                                        : nullptr;
            shooter->m_bulletMaterial = m_assets->m_allMaterials.count("Mat_Wall")
                                            ? m_assets->m_allMaterials["Mat_Wall"]
                                            : nullptr;
            shooter->m_spawnCallback = [this](std::unique_ptr<GameObject> newObj) {
                this->AddObject(std::move(newObj));
            };
        }
    }

    for (auto &o : m_pendingObjects)
        o->Start();
    m_physics->OptimizeBroadPhase();

    m_currentScenePath = filepath;
    std::cout << "[SceneManager] Scene loaded successfully from: " << filepath<< "\n";
}
