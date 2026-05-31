#include "engine/EngineDependencies.h"
#include "engine/components/physics/SkeletalRagdollComponent.h"

SkeletalRagdollComponent::SkeletalRagdollComponent() {
	componentType = "SkeletalRagdollComponent";
}


json SkeletalRagdollComponent::Serialize() {
	json j = Component::Serialize();
	j["isTriggered"] = isTriggered;
	j["showDebugBones"] = showDebugBones;
	j["ragdollScale"] = m_ragdollScale;

	j["bonePelvis"] = bonePelvis; j["boneSpine"] = boneSpine; j["boneHead"] = boneHead;
	j["boneLThigh"] = boneLThigh; j["boneRThigh"] = boneRThigh; j["boneLCalf"] = boneLCalf; j["boneRCalf"] = boneRCalf;
	j["boneLArm"] = boneLArm; j["boneRArm"] = boneRArm;
	return j;
}


void SkeletalRagdollComponent::Deserialize(const json& j) {
	isTriggered = j.value("isTriggered", false);
	showDebugBones = j.value("showDebugBones", true);
	m_ragdollScale = j.value("ragdollScale", 5.0f);

	bonePelvis = j.value("bonePelvis", ""); boneSpine = j.value("boneSpine", ""); boneHead = j.value("boneHead", "");
	boneLThigh = j.value("boneLThigh", ""); boneRThigh = j.value("boneRThigh", ""); boneLCalf = j.value("boneLCalf", ""); boneRCalf = j.value("boneRCalf", "");
	boneLArm = j.value("boneLArm", ""); boneRArm = j.value("boneRArm", "");
}


std::vector<std::string> SkeletalRagdollComponent::Split(const std::string& str) {
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;
	while (std::getline(ss, item, ',')) {
		item.erase(std::remove(item.begin(), item.end(), ' '), item.end());
		std::transform(item.begin(), item.end(), item.begin(), ::tolower);
		if (!item.empty()) result.push_back(item);
	}
	return result;
}


std::string SkeletalRagdollComponent::AutoFindBoneName(const std::map<std::string, BoneInfo>& boneMap, const std::string& keywordString) {
	std::vector<std::string> keywords = Split(keywordString);
	for (const auto& [name, info] : boneMap) {
		std::string lowerName = name;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
		for (const auto& kw : keywords) {
			if (lowerName.find(kw) != std::string::npos) return name;
		}
	}
	return "";
}


void SkeletalRagdollComponent::AutoDetectUnmappedBones(const std::map<std::string, BoneInfo>& boneMap) {
	if (bonePelvis.empty()) bonePelvis = AutoFindBoneName(boneMap, kwPelvis);
	if (boneSpine.empty())  boneSpine = AutoFindBoneName(boneMap, kwSpine);
	if (boneHead.empty())   boneHead = AutoFindBoneName(boneMap, kwHead);
	if (boneLThigh.empty()) boneLThigh = AutoFindBoneName(boneMap, kwLThigh);
	if (boneRThigh.empty()) boneRThigh = AutoFindBoneName(boneMap, kwRThigh);
	if (boneLCalf.empty())  boneLCalf = AutoFindBoneName(boneMap, kwLCalf);
	if (boneRCalf.empty())  boneRCalf = AutoFindBoneName(boneMap, kwRCalf);
	if (boneLArm.empty())   boneLArm = AutoFindBoneName(boneMap, kwLArm);
	if (boneRArm.empty())   boneRArm = AutoFindBoneName(boneMap, kwRArm);
}


void SkeletalRagdollComponent::BeginOverlap(GameObject* other) {
	if (!isTriggered && other->name.find("Bullet") != std::string::npos) {
		float dx = static_cast<float>(gameObject->transform.position.x - other->transform.position.x);
		float dy = static_cast<float>(gameObject->transform.position.y - other->transform.position.y);
		float dz = static_cast<float>(gameObject->transform.position.z - other->transform.position.z);
		SM::Vector3 hitDir(dx, dy, dz);
		hitDir.Normalize();
		TriggerRagdoll(hitDir);
	}
}


void SkeletalRagdollComponent::TriggerRagdoll(SM::Vector3 hitDir) {
	if (isTriggered || !m_physics) return;

	auto anim = gameObject->GetComponent<Animator>();
	if (!anim || !anim->skelMesh) return;

	// auto detect unmapped bones based on heuristic keywords, if they are not already mapped by explicit names
	AutoDetectUnmappedBones(anim->skelMesh->boneInfoMap);

	isTriggered = true;
	anim->isPlaying = false;
	showDebugBones = false;

	if (auto col = gameObject->GetComponent<Collider>()) col->Destroy();
	if (auto rb = gameObject->GetComponent<Rigidbody>()) rb->Destroy();

	JPH::BodyInterface& bi = m_physics->GetBodyInterface();
	JPH::ObjectLayer layer = 1;

	auto createCapsule = [&](JPH::Vec3 offset, float halfHeight, float radius, float mass) -> JPH::Body* {
		SM::Vector3 pos = SM::Vector3(static_cast<float>(gameObject->transform.position.x), static_cast<float>(gameObject->transform.position.y), static_cast<float>(gameObject->transform.position.z));
		JPH::BodyCreationSettings bcs(new JPH::CapsuleShape(halfHeight, radius), JPH::Vec3(pos.x, pos.y, pos.z) + offset, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, layer);
		bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
		bcs.mMassPropertiesOverride.mMass = mass;
		bcs.mCollisionGroup.SetGroupID(100);
		JPH::Body* b = bi.CreateBody(bcs);
		bi.AddBody(b->GetID(), JPH::EActivation::Activate);
		return b;
		};
	auto createSphere = [&](JPH::Vec3 offset, float radius, float mass) -> JPH::Body* {
		SM::Vector3 pos = SM::Vector3(static_cast<float>(gameObject->transform.position.x), static_cast<float>(gameObject->transform.position.y), static_cast<float>(gameObject->transform.position.z));
		JPH::BodyCreationSettings bcs(new JPH::SphereShape(radius), JPH::Vec3(pos.x, pos.y, pos.z) + offset, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, layer);
		bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
		bcs.mMassPropertiesOverride.mMass = mass;
		bcs.mCollisionGroup.SetGroupID(100);
		JPH::Body* b = bi.CreateBody(bcs);
		bi.AddBody(b->GetID(), JPH::EActivation::Activate);
		return b;
		};

	auto& bMap = anim->skelMesh->boneInfoMap;

	// id and offset matrix of each bone for mapping physics bodies to bones later. 
	// If a bone is not mapped, its id will be -1 and offset will be identity matrix (so it will just follow the parent bone).
	auto getBoneData = [&](const std::string& bName, int& outID, SM::Matrix& outOffset) {
		if (bMap.count(bName)) {
			outID = bMap.at(bName).id;
			outOffset = bMap.at(bName).offsetMatrix;
		}
		else {
			outID = -1; outOffset = SM::Matrix::Identity;
		}
		};

	int bP = -1, bS = -1, bH = -1, bLT = -1, bRT = -1, bLC = -1, bRC = -1, bLA = -1, bRA = -1;
	SM::Matrix oP, oS, oH, oLT, oRT, oLC, oRC, oLA, oRA;

	getBoneData(bonePelvis, bP, oP); getBoneData(boneSpine, bS, oS); getBoneData(boneHead, bH, oH);
	getBoneData(boneLThigh, bLT, oLT); getBoneData(boneRThigh, bRT, oRT); getBoneData(boneLCalf, bLC, oLC); getBoneData(boneRCalf, bRC, oRC);
	getBoneData(boneLArm, bLA, oLA); getBoneData(boneRArm, bRA, oRA);

	if (bP == -1) {
		std::cout << "[SKELETAL RAGDOLL] Chyba: Pelvis bone neni namapovan. Ragdoll nemuze startovat!\n";
		return;
	}

	float S = m_ragdollScale;

	JPH::Body* idPelvis = createCapsule(JPH::Vec3(0, 1.0f * S, 0), 0.15f * S, 0.2f * S, 20.0f);
	JPH::Body* idSpine = createCapsule(JPH::Vec3(0, 1.3f * S, 0), 0.15f * S, 0.2f * S, 15.0f);
	JPH::Body* idHead = createSphere(JPH::Vec3(0, 1.6f * S, 0), 0.1f * S, 5.0f);
	JPH::Body* idLThigh = createCapsule(JPH::Vec3(-0.2f * S, 0.7f * S, 0), 0.2f * S, 0.1f * S, 10.0f);
	JPH::Body* idRThigh = createCapsule(JPH::Vec3(0.2f * S, 0.7f * S, 0), 0.2f * S, 0.1f * S, 10.0f);
	JPH::Body* idLCalf = createCapsule(JPH::Vec3(-0.2f * S, 0.3f * S, 0), 0.2f * S, 0.1f * S, 5.0f);
	JPH::Body* idRCalf = createCapsule(JPH::Vec3(0.2f * S, 0.3f * S, 0), 0.2f * S, 0.1f * S, 5.0f);
	JPH::Body* idLArm = createCapsule(JPH::Vec3(-0.4f * S, 1.3f * S, 0), 0.2f * S, 0.08f * S, 5.0f);
	JPH::Body* idRArm = createCapsule(JPH::Vec3(0.4f * S, 1.3f * S, 0), 0.2f * S, 0.08f * S, 5.0f);

	m_pelvisID = idPelvis->GetID();

	auto addMapping = [&](int boneIdx, JPH::Body* body, SM::Matrix offset) {
		if (boneIdx != -1) m_mappedBones.push_back({ boneIdx, body->GetID(), offset });
		};
	addMapping(bP, idPelvis, oP); addMapping(bS, idSpine, oS); addMapping(bH, idHead, oH);
	addMapping(bLT, idLThigh, oLT); addMapping(bRT, idRThigh, oRT); addMapping(bLC, idLCalf, oLC); addMapping(bRC, idRCalf, oRC);
	addMapping(bLA, idLArm, oLA); addMapping(bRA, idRArm, oRA);

	auto createJoint = [&](JPH::Body* b1, JPH::Body* b2, JPH::Vec3 pivot) {
		SM::Vector3 pos = SM::Vector3(static_cast<float>(gameObject->transform.position.x), static_cast<float>(gameObject->transform.position.y), static_cast<float>(gameObject->transform.position.z));
		JPH::PointConstraintSettings joint;
		joint.mSpace = JPH::EConstraintSpace::WorldSpace;
		joint.mPoint1 = joint.mPoint2 = JPH::Vec3(pos.x, pos.y, pos.z) + pivot;
		m_physics->AddConstraint(joint.Create(*b1, *b2));
		};

	createJoint(idPelvis, idSpine, JPH::Vec3(0, 1.15f * S, 0));
	createJoint(idSpine, idHead, JPH::Vec3(0, 1.45f * S, 0));
	createJoint(idPelvis, idLThigh, JPH::Vec3(-0.2f * S, 0.9f * S, 0));
	createJoint(idPelvis, idRThigh, JPH::Vec3(0.2f * S, 0.9f * S, 0));
	createJoint(idLThigh, idLCalf, JPH::Vec3(-0.2f * S, 0.5f * S, 0));
	createJoint(idRThigh, idRCalf, JPH::Vec3(0.2f * S, 0.5f * S, 0));
	createJoint(idSpine, idLArm, JPH::Vec3(-0.35f * S, 1.35f * S, 0));
	createJoint(idSpine, idRArm, JPH::Vec3(0.35f * S, 1.35f * S, 0));

	JPH::Vec3 force(hitDir.x * 30000.0f, hitDir.y * 30000.0f + 10000.0f, hitDir.z * 30000.0f);
	bi.AddImpulse(idSpine->GetID(), force);
}


void SkeletalRagdollComponent::Update(float dt) {
	if (!isTriggered || !m_physics || m_mappedBones.empty()) return;

	auto anim = gameObject->GetComponent<Animator>();
	if (!anim) return;

	JPH::BodyInterface& bi = m_physics->GetBodyInterface();

	if (!m_pelvisID.IsInvalid() && bi.IsAdded(m_pelvisID)) {
		JPH::RVec3 pPos; JPH::Quat pRot;
		bi.GetPositionAndRotation(m_pelvisID, pPos, pRot);
		SM::Vector3 joltPos(static_cast<float>(pPos.GetX()), static_cast<float>(pPos.GetY()), static_cast<float>(pPos.GetZ()));
		SM::Vector3 offset(0.0f, -1.0f * m_ragdollScale, 0.0f);
		gameObject->transform.position = { joltPos.x + offset.x, joltPos.y + offset.y, joltPos.z + offset.z };
	}

	SM::Matrix worldMatrix = SM::Matrix::CreateScale(gameObject->transform.scale) * SM::Matrix::CreateFromQuaternion(gameObject->transform.rotation) * SM::Matrix::CreateTranslation(static_cast<float>(gameObject->transform.position.x), static_cast<float>(gameObject->transform.position.y), static_cast<float>(gameObject->transform.position.z));
	SM::Matrix worldToLocal = worldMatrix.Invert();

	for (const auto& part : m_mappedBones) {
		if (!part.bodyID.IsInvalid() && bi.IsAdded(part.bodyID)) {
			JPH::RVec3 p; JPH::Quat q;
			bi.GetPositionAndRotation(part.bodyID, p, q);
			SM::Matrix joltWorld = SM::Matrix::CreateFromQuaternion(SM::Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW())) *
				SM::Matrix::CreateTranslation(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
			SM::Matrix localBoneTransform = joltWorld * worldToLocal;
			anim->finalBoneMatrices[part.boneID] = localBoneTransform * part.offsetMatrix;
		}
	}
}


void SkeletalRagdollComponent::DrawBoneCombo(const char* label, std::string& currentBone, const std::map<std::string, BoneInfo>& boneMap) {
	ImGui::PushID(label);
	ImGui::Text("%s", label); ImGui::SameLine(100);

	if (currentBone.empty()) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
	else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));

	std::string comboLabel = currentBone.empty() ? "--- UNMAPPED ---" : currentBone;
	ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginCombo("##combo", comboLabel.c_str())) {
		if (ImGui::Selectable("--- UNMAPPED ---", currentBone.empty())) currentBone = "";
		for (const auto& [name, info] : boneMap) {
			bool isSelected = (currentBone == name);
			if (ImGui::Selectable(name.c_str(), isSelected)) currentBone = name;
			if (isSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleColor();
	ImGui::PopID();
}


void SkeletalRagdollComponent::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Skeletal Ragdoll Setup");

	auto anim = gameObject->GetComponent<Animator>();
	if (!anim || !anim->skelMesh) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Missing Animator or SkinnedMesh!");
		return;
	}

	ImGui::SliderFloat("Scale", &m_ragdollScale, 1.0f, 20.0f);
	ImGui::Checkbox("Show FBX bones (yellow spheres)", &showDebugBones);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Manual Bone Mapping:");

	if (ImGui::Button("Auto-Detect Unmapped", ImVec2(-1, 0))) {
		AutoDetectUnmappedBones(anim->skelMesh->boneInfoMap);
	}

	auto& bMap = anim->skelMesh->boneInfoMap;
	DrawBoneCombo("Pelvis", bonePelvis, bMap);
	DrawBoneCombo("Spine", boneSpine, bMap);
	DrawBoneCombo("Head", boneHead, bMap);
	DrawBoneCombo("L Thigh", boneLThigh, bMap);
	DrawBoneCombo("R Thigh", boneRThigh, bMap);
	DrawBoneCombo("L Calf", boneLCalf, bMap);
	DrawBoneCombo("R Calf", boneRCalf, bMap);
	DrawBoneCombo("L Arm", boneLArm, bMap);
	DrawBoneCombo("R Arm", boneRArm, bMap);

	ImGui::Separator();
	if (!isTriggered && m_physics) {
		if (ImGui::Button("Test: kill!", ImVec2(-1, 30))) {
			TriggerRagdoll(SM::Vector3(0.0f, 0.5f, 1.0f));
		}
	}
}
