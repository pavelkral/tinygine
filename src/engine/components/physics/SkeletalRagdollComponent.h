#pragma once

#include "pch/Pch.h"

struct RagdollBonePart {
    int boneID = -1;
    JPH::BodyID bodyID;
    SM::Matrix offsetMatrix;
};

class SkeletalRagdollComponent : public Component {
public:
    JPH::PhysicsSystem* m_physics = nullptr;
    std::vector<RagdollBonePart> m_mappedBones;
    JPH::BodyID m_pelvisID;
    bool isTriggered = false;
    bool showDebugBones = true;
    float m_ragdollScale = 5.0f;

	// bone names for ragdoll mapping (stored in level, can be overridden by auto-detection)
    std::string bonePelvis = "";
    std::string boneSpine = "";
    std::string boneHead = "";
    std::string boneLThigh = "";
    std::string boneRThigh = "";
    std::string boneLCalf = "";
    std::string boneRCalf = "";
    std::string boneLArm = "";
    std::string boneRArm = "";

	// heuristic keywords for auto-detecting bone names (not stored in level, just used for auto-mapping)
    std::string kwPelvis = "pelvis,hips";
    std::string kwSpine = "spine";
    std::string kwHead = "head";
    std::string kwLThigh = "leftupleg,thigh_l,l_thigh";
    std::string kwRThigh = "rightupleg,thigh_r,r_thigh";
    std::string kwLCalf = "leftleg,calf_l,l_calf";
    std::string kwRCalf = "rightleg,calf_r,r_calf";
    std::string kwLArm = "leftarm,upperarm_l,l_upperarm";
    std::string kwRArm = "rightarm,upperarm_r,r_upperarm";    
    SkeletalRagdollComponent();    
    json Serialize() override;    
    void Deserialize(const json& j) override;    
    std::vector<std::string> Split(const std::string& str);    
    std::string AutoFindBoneName(const std::map<std::string, BoneInfo>& boneMap, const std::string& keywordString);    
    void AutoDetectUnmappedBones(const std::map<std::string, BoneInfo>& boneMap);    
    void BeginOverlap(GameObject* other) override;    
    void TriggerRagdoll(SM::Vector3 hitDir);    
    void Update(float dt) override;    
    void DrawBoneCombo(const char* label, std::string& currentBone, const std::map<std::string, BoneInfo>& boneMap);    
    void OnGUI() override;
};


