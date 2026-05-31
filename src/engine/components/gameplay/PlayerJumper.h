#pragma once

#include "pch/Pch.h"

class PlayerJumper : public Component {
private:
    Rigidbody* m_rb = nullptr;
    float m_jumpImpulse = 111500.0f;

public:    
    PlayerJumper();    
    json Serialize() override;    
    void Deserialize(const json& j) override;    
    void Start() override;    
    void Update(float dt) override;    
    void OnGUI() override;
};

