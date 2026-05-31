#pragma once

#include "pch/Pch.h"

class Animator : public Component {
public:
    std::shared_ptr<SkeletalMesh> skelMesh;
    std::vector<SM::Matrix> finalBoneMatrices;
    float currentTime = 0.0f, startTick = 0.0f, endTick = 0.0f;
    int currentAnimIndex = -1;
    bool isPlaying = false, loopAnim = true;    Animator(std::shared_ptr<SkeletalMesh> mesh = nullptr);    json Serialize() override;    void Deserialize(const json& j) override;    void Play(int animIndex, float startTime = 0.0f, float endTime = -1.0f, bool playLoop = true);    void Start() override;    void Update(float dt) override;
private:    SM::Vector3 GetInterpPos(float time, const aiNodeAnim* ch);    SM::Quaternion GetInterpRot(float time, const aiNodeAnim* ch);    SM::Vector3 GetInterpScale(float time, const aiNodeAnim* ch);    void CalculateBoneTransform(const aiNode* node, SM::Matrix parentTransform, const aiAnimation* anim);
};
// ============================================================================
// PHYSICS COMPONENTS
// ============================================================================
