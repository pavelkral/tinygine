#include "engine/physics/PhysicsLayers.h"
#include "engine/EngineDependencies.h"

BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
}

uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return BroadPhaseLayers::NUM_LAYERS;
}

BroadPhaseLayer
BPLayerInterfaceImpl::GetBroadPhaseLayer(ObjectLayer inLayer) const {
    return mObjectToBroadPhase[inLayer];
}

const char *
BPLayerInterfaceImpl::GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const {
    return inLayer == BroadPhaseLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
}

bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(
    ObjectLayer l1, BroadPhaseLayer l2) const {
    return (l1 == Layers::NON_MOVING) ? (l2 == BroadPhaseLayers::MOVING) : true;
}

bool ObjectLayerPairFilterImpl::ShouldCollide(ObjectLayer o1,
                                              ObjectLayer o2) const {
    return (o1 == Layers::NON_MOVING) ? (o2 == Layers::MOVING) : true;
}
