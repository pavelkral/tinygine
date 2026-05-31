#pragma once

#include "pch/Pch.h"
#include "engine/scene/Transform.h"

class GameObject;

class Component {
public:
    GameObject* gameObject = nullptr;
    bool isPendingDestroy = false;
    std::string componentType = "Component";

    virtual ~Component();

    virtual void Awake();
    virtual void Start();
    virtual void Update(float dt);
    virtual void FixedUpdate(float fixedDt);
    virtual void BeginOverlap(GameObject* other);
    virtual void EndOverlap(GameObject* other);
    virtual void Reset();
    virtual void OnGUI();
    void Destroy();
    virtual json Serialize();
    virtual void Deserialize(const json& j);
};
