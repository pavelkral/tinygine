#include "engine/EngineDependencies.h"
#include "engine/scene/Component.h"

Component::~Component() = default;

void Component::Awake() {}

void Component::Start() {}

void Component::Update(float dt) {}

void Component::FixedUpdate(float fixedDt) {}

void Component::BeginOverlap(GameObject* other) {}

void Component::EndOverlap(GameObject* other) {}

void Component::Reset() {}

void Component::OnGUI() {}

void Component::Destroy() {
    isPendingDestroy = true;
}

json Component::Serialize() {
    json j;
    j["type"] = componentType;
    return j;
}

void Component::Deserialize(const json& j) {}
