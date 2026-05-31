#include "engine/EngineDependencies.h"
#include "engine/physics/MyContactListener.h"

MyContactListener::MyContactListener(JPH::PhysicsSystem* ps) : m_physicsSystem(ps) {}


void MyContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
	GameObject* obj1 = reinterpret_cast<GameObject*>(inBody1.GetUserData());
	GameObject* obj2 = reinterpret_cast<GameObject*>(inBody2.GetUserData());

	if (obj1 && obj2) {
		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_events.push_back({ CollisionEventType::Enter, obj1, obj2 });
	}
}


void MyContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {}


void MyContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) {}


void MyContactListener::ProcessEvents() {
	std::lock_guard<std::mutex> lock(m_eventMutex);
	for (const auto& ev : m_events) {
		if (ev.type == CollisionEventType::Enter) {
			ev.obj1->OnCollisionEnter(ev.obj2);
			ev.obj2->OnCollisionEnter(ev.obj1);
		}
	}
	m_events.clear();
}
