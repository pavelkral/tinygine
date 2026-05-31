#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/Brick.h"

Brick::Brick(ma_engine* audio) : audioEngine(audio) { componentType = "Brick"; }


json Brick::Serialize() {
	json j = Component::Serialize();
	j["isBroken"] = isBroken;
	j["deathTimer"] = deathTimer;
	return j;
}


void Brick::Deserialize(const json& j) {
	isBroken = j.value("isBroken", false);
	deathTimer = j.value("deathTimer", 1.0f);
}


void Brick::BeginOverlap(GameObject* other) {
	if (isBroken) return;

	if (other->name.find("Ball") != std::string::npos) {
		isBroken = true;

		if (audioEngine) {
			ma_engine_play_sound(audioEngine, "assets/audio/music2.wav", NULL);
		}

		if (auto ps = gameObject->GetComponent<ParticleSystemComponent>()) {
			ps->Play();
		}

		if (auto rb = gameObject->GetComponent<Rigidbody>()) {
			rb->bodyInterface->SetMotionType(rb->bodyID, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
			rb->bodyInterface->SetObjectLayer(rb->bodyID, Layers::MOVING);

			rb->isDynamic = true;

			rb->bodyInterface->SetLinearVelocity(rb->bodyID, JPH::Vec3(0.0f, -15.0f, 5.0f));
			rb->bodyInterface->SetAngularVelocity(rb->bodyID, JPH::Vec3(10.0f, 5.0f, 0.0f));
		}
	}
}


void Brick::Update(float dt) {
	if (isBroken) {
		deathTimer -= dt;
		if (deathTimer <= 0.0f) {
			Destroy();
		}
	}
}
