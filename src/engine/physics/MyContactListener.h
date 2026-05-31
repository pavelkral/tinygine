#pragma once

#include "pch/Pch.h"

class MyContactListener : public JPH::ContactListener {
private:
    JPH::PhysicsSystem* m_physicsSystem;
    std::mutex m_eventMutex;
    std::vector<CollisionEvent> m_events;

public:    
    MyContactListener(JPH::PhysicsSystem* ps);    
    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;    
    virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;    
    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;    
    void ProcessEvents();
};






