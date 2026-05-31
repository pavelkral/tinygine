#pragma once

#include "pch/Pch.h"
#include "engine/scene/Component.h"

class GameObject {
public:
    std::string name;
    Transform transform;
    std::vector<std::unique_ptr<Component>> components;
    bool isPendingDestroy = false;
    bool isStarted = false;

    GameObject(std::string n);

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        comp->gameObject = this;
        components.push_back(std::move(comp));
        ptr->Awake();

        if (isStarted) {
            ptr->Start();
        }
        return ptr;
    }

    template<typename T>
    T* GetComponent() {
        for (auto& c : components) {
            if (T* ptr = dynamic_cast<T*>(c.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    void Start();
    void Update(float dt);
    void Cleanup();
    void FixedUpdate(float fixedDt);
    void Reset();
    void OnCollisionEnter(GameObject* other);
    void OnCollisionExit(GameObject* other);
    void Destroy();
    json Serialize();
    void DeserializeBase(const json& j);
};
