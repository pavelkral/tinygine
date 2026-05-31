#include "engine/EngineDependencies.h"
#include "engine/components/physics/Rigidbody.h"

Rigidbody::Rigidbody(JPH::BodyInterface* bi, bool dyn) : bodyInterface(bi), isDynamic(dyn) {
	componentType = "Rigidbody";
}


json Rigidbody::Serialize() {
	json j = Component::Serialize();
	j["isDynamic"] = isDynamic;
	j["useCustomMass"] = useCustomMass;
	j["mass"] = mass;
	j["friction"] = friction;
	j["restitution"] = restitution;
	return j;
}


void Rigidbody::Deserialize(const json& j) {
	isDynamic = j.value("isDynamic", false);
	useCustomMass = j.value("useCustomMass", false);
	mass = j.value("mass", 1.0f);
	friction = j.value("friction", 0.2f);
	restitution = j.value("restitution", 0.0f);
}


void Rigidbody::InitBody() {
	if (!bodyID.IsInvalid()) return;

	Collider* col = gameObject->GetComponent<Collider>();
	if (!col) return;

	JPH::Ref<JPH::Shape> shape = col->CreateShape();
	initialPos = gameObject->transform.position;
	initialRot = gameObject->transform.rotation;

	Vector3d localPhysicsPos = gameObject->transform.position - g_physicsOrigin;
	JPH::Quat joltRot(initialRot.x, initialRot.y, initialRot.z, initialRot.w);

	JPH::BodyCreationSettings s(
		shape,
		JPH::RVec3((float)localPhysicsPos.x, (float)localPhysicsPos.y, (float)localPhysicsPos.z),
		joltRot,
		isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
		isDynamic ? Layers::MOVING : Layers::NON_MOVING
	);

	s.mUserData = reinterpret_cast<uint64_t>(gameObject);

	// detect dynamic or kinematic body and set appropriate settings
	s.mAllowDynamicOrKinematic = true;

	s.mFriction = friction;
	s.mRestitution = restitution;

	// set custom mass
	if (isDynamic && useCustomMass) {
		s.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		s.mMassPropertiesOverride.mMass = mass;
	}

	bodyID = bodyInterface->CreateAndAddBody(s, isDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}


void Rigidbody::Start() {
	InitBody();
}


void Rigidbody::Reset() {
	if (bodyID.IsInvalid() || !isDynamic) return;

	Vector3d localPhysicsPos = initialPos - g_physicsOrigin;
	bodyInterface->SetPositionAndRotation(
		bodyID,
		JPH::RVec3((float)localPhysicsPos.x, (float)localPhysicsPos.y, (float)localPhysicsPos.z),
		JPH::Quat(initialRot.x, initialRot.y, initialRot.z, initialRot.w),
		JPH::EActivation::Activate
	);

	bodyInterface->SetLinearAndAngularVelocity(bodyID, JPH::Vec3::sZero(), JPH::Vec3::sZero());
	gameObject->transform.position = initialPos;
	gameObject->transform.rotation = initialRot;
}


void Rigidbody::Update(float dt) {
	if (bodyID.IsInvalid() || !isDynamic) return;

	JPH::RVec3 p;
	JPH::Quat q;
	bodyInterface->GetPositionAndRotation(bodyID, p, q);

	Vector3d localPhysicsPos(p.GetX(), p.GetY(), p.GetZ());
	gameObject->transform.position = localPhysicsPos + g_physicsOrigin;
	gameObject->transform.rotation = SM::Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}


void Rigidbody::RecreateShape() {
	if (!bodyInterface) return;

	Collider* col = gameObject->GetComponent<Collider>();
	if (!col) return;

	if (bodyID.IsInvalid()) {
		InitBody();
	}
	else {
		JPH::Ref<JPH::Shape> newShape = col->CreateShape();
		bodyInterface->SetShape(bodyID, newShape, true, JPH::EActivation::DontActivate);
	}
}


Rigidbody::~Rigidbody() {
	if (!bodyID.IsInvalid() && bodyInterface) {
		bodyInterface->RemoveBody(bodyID);
		bodyInterface->DestroyBody(bodyID);
	}
}


void Rigidbody::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Rigidbody");
	ImGui::SameLine();
	if (ImGui::Button("Remove##rb")) Destroy();

	ImGui::Checkbox("Dynamic", &isDynamic);

	if (isDynamic) {
		ImGui::Checkbox("Use Custom Mass", &useCustomMass);
		if (useCustomMass) {
			ImGui::SliderFloat("Mass (kg)", &mass, 0.1f, 500.0f, "%.1f");
		}
	}

	if (ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f, "%.2f")) {
		if (!bodyID.IsInvalid() && bodyInterface) {
			bodyInterface->SetRestitution(bodyID, restitution);
		}
	}

	if (ImGui::SliderFloat("Friction", &friction, 0.0f, 1.0f, "%.2f")) {
		if (!bodyID.IsInvalid() && bodyInterface) {
			bodyInterface->SetFriction(bodyID, friction);
		}
	}
}
