#include "engine/EngineDependencies.h"
#include "engine/components/render/Animator.h"

Animator::Animator(std::shared_ptr<SkeletalMesh> mesh) : skelMesh(mesh) {
        componentType = "Animator";
        finalBoneMatrices.resize(MAX_BONES, SM::Matrix::Identity);
    }


json Animator::Serialize() {
        json j = Component::Serialize();
        j["isPlaying"] = isPlaying;
        j["loopAnim"] = loopAnim;
        j["currentAnimIndex"] = currentAnimIndex;
        j["currentTime"] = currentTime;
        j["startTick"] = startTick;
        j["endTick"] = endTick;
        return j;
    }


void Animator::Deserialize(const json& j) {
        isPlaying = j.value("isPlaying", false);
        loopAnim = j.value("loopAnim", true);
        currentAnimIndex = j.value("currentAnimIndex", -1);
        currentTime = j.value("currentTime", 0.0f);
        startTick = j.value("startTick", 0.0f);
        endTick = j.value("endTick", 0.0f);
    }


void Animator::Play(int animIndex, float startTime, float endTime, bool playLoop) {
        if (!skelMesh || !skelMesh->scene || skelMesh->scene->mNumAnimations <= (unsigned)animIndex) return;
        currentAnimIndex = animIndex; loopAnim = playLoop; auto anim = skelMesh->scene->mAnimations[animIndex];
        startTick = startTime; endTick = (endTime < 0.0f) ? (float)anim->mDuration : endTime; currentTime = startTick; isPlaying = true;
    }


void Animator::Start() {
        Update(0.0f);
    }


void Animator::Update(float dt) {
        if (!skelMesh || !skelMesh->scene || !skelMesh->scene->mRootNode) return;
        const aiAnimation* animToPlay = nullptr;
        if (isPlaying && currentAnimIndex >= 0) {
            animToPlay = skelMesh->scene->mAnimations[currentAnimIndex];
            float tps = (float)(animToPlay->mTicksPerSecond != 0 ? animToPlay->mTicksPerSecond : 25.0f);
            currentTime += dt * tps;
            if (currentTime >= endTick) {
                if (loopAnim) {
                    if (endTick > startTick) currentTime = startTick + fmod(currentTime - startTick, endTick - startTick);
                    else currentTime = startTick;
                }
                else {
                    currentTime = endTick; isPlaying = false;
                }
            }
        }
        CalculateBoneTransform(skelMesh->scene->mRootNode, SM::Matrix::Identity, animToPlay);
    }


SM::Vector3 Animator::GetInterpPos(float time, const aiNodeAnim* ch) {
        if (ch->mNumPositionKeys == 1) return { ch->mPositionKeys[0].mValue.x, ch->mPositionKeys[0].mValue.y, ch->mPositionKeys[0].mValue.z };
        for (unsigned int i = 0; i < ch->mNumPositionKeys - 1; i++) {
            if (time < (float)ch->mPositionKeys[i + 1].mTime) {
                float factor = (time - (float)ch->mPositionKeys[i].mTime) / ((float)ch->mPositionKeys[i + 1].mTime - (float)ch->mPositionKeys[i].mTime);
                SM::Vector3 s(ch->mPositionKeys[i].mValue.x, ch->mPositionKeys[i].mValue.y, ch->mPositionKeys[i].mValue.z);
                SM::Vector3 e(ch->mPositionKeys[i + 1].mValue.x, ch->mPositionKeys[i + 1].mValue.y, ch->mPositionKeys[i + 1].mValue.z);
                return SM::Vector3::Lerp(s, e, factor);
            }
        }
        auto last = ch->mPositionKeys[ch->mNumPositionKeys - 1].mValue; return { last.x, last.y, last.z };
    }


SM::Quaternion Animator::GetInterpRot(float time, const aiNodeAnim* ch) {
        if (ch->mNumRotationKeys == 1) return { ch->mRotationKeys[0].mValue.x, ch->mRotationKeys[0].mValue.y, ch->mRotationKeys[0].mValue.z, ch->mRotationKeys[0].mValue.w };
        for (unsigned int i = 0; i < ch->mNumRotationKeys - 1; i++) {
            if (time < (float)ch->mRotationKeys[i + 1].mTime) {
                float factor = (time - (float)ch->mRotationKeys[i].mTime) / ((float)ch->mRotationKeys[i + 1].mTime - (float)ch->mRotationKeys[i].mTime);
                SM::Quaternion s(ch->mRotationKeys[i].mValue.x, ch->mRotationKeys[i].mValue.y, ch->mRotationKeys[i].mValue.z, ch->mRotationKeys[i].mValue.w);
                SM::Quaternion e(ch->mRotationKeys[i + 1].mValue.x, ch->mRotationKeys[i + 1].mValue.y, ch->mRotationKeys[i + 1].mValue.z, ch->mRotationKeys[i + 1].mValue.w);
                return SM::Quaternion::Slerp(s, e, factor);
            }
        }
        auto last = ch->mRotationKeys[ch->mNumRotationKeys - 1].mValue; return { last.x, last.y, last.z, last.w };
    }


SM::Vector3 Animator::GetInterpScale(float time, const aiNodeAnim* ch) {
        if (ch->mNumScalingKeys == 1) return { ch->mScalingKeys[0].mValue.x, ch->mScalingKeys[0].mValue.y, ch->mScalingKeys[0].mValue.z };
        for (unsigned int i = 0; i < ch->mNumScalingKeys - 1; i++) {
            if (time < (float)ch->mScalingKeys[i + 1].mTime) {
                float factor = (time - (float)ch->mScalingKeys[i].mTime) / ((float)ch->mScalingKeys[i + 1].mTime - (float)ch->mScalingKeys[i].mTime);
                SM::Vector3 s(ch->mScalingKeys[i].mValue.x, ch->mScalingKeys[i].mValue.y, ch->mScalingKeys[i].mValue.z);
                SM::Vector3 e(ch->mScalingKeys[i + 1].mValue.x, ch->mScalingKeys[i + 1].mValue.y, ch->mScalingKeys[i + 1].mValue.z);
                return SM::Vector3::Lerp(s, e, factor);
            }
        }
        auto last = ch->mScalingKeys[ch->mNumScalingKeys - 1].mValue; return { last.x, last.y, last.z };
    }


void Animator::CalculateBoneTransform(const aiNode* node, SM::Matrix parentTransform, const aiAnimation* anim) {
        std::string nodeName = node->mName.data;
        SM::Matrix nodeTransform = AssimpToSimpleMathMatrix(node->mTransformation);

        if (anim) {
            const aiNodeAnim* nodeAnim = nullptr;
            for (unsigned int i = 0; i < anim->mNumChannels; i++) {
                if (anim->mChannels[i]->mNodeName.data == nodeName) {
                    nodeAnim = anim->mChannels[i]; break;
                }
            }
            if (nodeAnim && nodeAnim->mNumPositionKeys > 0) {
                SM::Vector3 pos = GetInterpPos(currentTime, nodeAnim);
                SM::Quaternion rot = GetInterpRot(currentTime, nodeAnim);
                SM::Vector3 scale = GetInterpScale(currentTime, nodeAnim);

                nodeTransform = SM::Matrix::CreateScale(scale) * SM::Matrix::CreateFromQuaternion(rot) * SM::Matrix::CreateTranslation(pos);
            }
        }

        SM::Matrix globalTransform = nodeTransform * parentTransform;

        if (skelMesh->boneInfoMap.find(nodeName) != skelMesh->boneInfoMap.end()) {
            int boneID = skelMesh->boneInfoMap[nodeName].id;
            SM::Matrix offset = skelMesh->boneInfoMap[nodeName].offsetMatrix;

            finalBoneMatrices[boneID] = (offset * globalTransform).Transpose();
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            CalculateBoneTransform(node->mChildren[i], globalTransform, anim);
    }
