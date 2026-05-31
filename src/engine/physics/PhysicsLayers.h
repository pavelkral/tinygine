#pragma once

#include "pch/Pch.h"

namespace fs = std::filesystem;
using namespace JPH;

// ============================================================================
// JOLT PHYSICS LAYERS & FILTERS
// ============================================================================

namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
};

// Maps object layers to broadphase layers
class BPLayerInterfaceImpl : public BroadPhaseLayerInterface {
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
public:    
    BPLayerInterfaceImpl();    
    virtual uint GetNumBroadPhaseLayers() const override;    
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override;    
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override;
};

// Determines if an object layer should collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:    
    virtual bool ShouldCollide(ObjectLayer l1, BroadPhaseLayer l2) const override;
};

// Determines if two object layers should collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:    
    virtual bool ShouldCollide(ObjectLayer o1, ObjectLayer o2) const override;
};


