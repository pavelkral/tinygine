#pragma once

#include "pch/Pch.h"
#include "engine/scene/Component.h"
#include "engine/scene/SceneTypes.h"

class Rigidbody : public Component {
public:
    JPH::BodyInterface* bodyInterface;
    JPH::BodyID bodyID;
    bool isDynamic;
    Vector3d initialPos;
    SM::Quaternion initialRot;

    bool useCustomMass = false;
    float mass = 1.0f;

    float friction = 0.2f;
    float restitution = 0.0f;

    Rigidbody(JPH::BodyInterface* bi, bool dyn);
    ~Rigidbody() override;

    json Serialize() override;
    void Deserialize(const json& j) override;
    void InitBody();
    void Start() override;
    void Reset() override;
    void Update(float dt) override;
    void RecreateShape();
    void OnGUI() override;
};
