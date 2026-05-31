#include "engine/EngineDependencies.h"
#include "engine/components/gameplay/DeathZone.h"

DeathZone::DeathZone() { componentType = "DeathZone"; }


json DeathZone::Serialize() { return Component::Serialize(); }


void DeathZone::Deserialize(const json& j) {}


void DeathZone::BeginOverlap(GameObject* other) {
	if (other->name.find("Ball") != std::string::npos) {
		other->Destroy();
		if (paddleRef) paddleRef->ballSpawned = false;
		std::cout << "ZTRATIL JSI ZIVOT!\n";
	}
	else if (other->name.find("Brick") != std::string::npos) {
		other->Destroy();
	}
}
