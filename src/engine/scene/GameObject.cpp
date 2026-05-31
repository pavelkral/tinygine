#include "engine/EngineDependencies.h"
#include "engine/scene/GameObject.h"

GameObject::GameObject(std::string n) : name(n) {}

void GameObject::Start() {
    if (isStarted) {
        return;
    }

    for (auto& c : components) {
        c->Start();
    }
    isStarted = true;
}

void GameObject::Update(float dt) {
    for (auto& c : components) {
        if (!c->isPendingDestroy) {
            c->Update(dt);
        }
    }
}

void GameObject::Cleanup() {
    components.erase(
        std::remove_if(
            components.begin(),
            components.end(),
            [](const std::unique_ptr<Component>& c) { return c->isPendingDestroy; }),
        components.end());
}

void GameObject::FixedUpdate(float fixedDt) {
    for (auto& c : components) {
        c->FixedUpdate(fixedDt);
    }
}

void GameObject::Reset() {
    for (auto& c : components) {
        c->Reset();
    }
}

void GameObject::OnCollisionEnter(GameObject* other) {
    for (auto& c : components) {
        c->BeginOverlap(other);
    }
}

void GameObject::OnCollisionExit(GameObject* other) {
    for (auto& c : components) {
        c->EndOverlap(other);
    }
}

void GameObject::Destroy() {
    isPendingDestroy = true;
}

json GameObject::Serialize() {
    json j;
    j["name"] = name;
    j["transform"]["position"] = { transform.position.x, transform.position.y, transform.position.z };
    j["transform"]["eulerAngles"] = { transform.eulerAngles.x, transform.eulerAngles.y, transform.eulerAngles.z };
    j["transform"]["scale"] = { transform.scale.x, transform.scale.y, transform.scale.z };

    json jComps = json::array();
    for (auto& c : components) {
        if (!c->isPendingDestroy) {
            jComps.push_back(c->Serialize());
        }
    }
    j["components"] = jComps;
    return j;
}

void GameObject::DeserializeBase(const json& j) {
    name = j.value("name", "UnknownObject");

    if (!j.contains("transform")) {
        return;
    }

    auto jt = j["transform"];
    if (jt.contains("position")) {
        transform.position = { jt["position"][0], jt["position"][1], jt["position"][2] };
    }
    if (jt.contains("eulerAngles")) {
        transform.eulerAngles = { jt["eulerAngles"][0], jt["eulerAngles"][1], jt["eulerAngles"][2] };
        transform.UpdateRotation();
    }
    if (jt.contains("scale")) {
        transform.scale = { jt["scale"][0], jt["scale"][1], jt["scale"][2] };
    }
}
