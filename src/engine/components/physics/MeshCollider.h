#pragma once

#include "pch/Pch.h"

class MeshCollider : public Collider {
public:
    std::shared_ptr<Mesh> collisionMesh;
    std::shared_ptr<SkeletalMesh> collisionSkelMesh;
    std::string meshName = "";
    bool isConvex = false;

    // NEW VARIABLES FOR DETAIL OPTIMIZATION (in meters)

	float hullTolerance = 0.001f;     // 1 mm tolerance for 
    //merging close vertices when creating convex hulls, to reduce vertex count and improve performance (can be increased for better performance at the cost of accuracy)
    float maxConvexRadius = 0.05f;       
    MeshCollider(std::shared_ptr<Mesh> mesh = nullptr, std::shared_ptr<SkeletalMesh> skelMesh = nullptr, std::string mName = "", bool convex = false);    
    json Serialize() override;    void Deserialize(const json& j) override;    
    JPH::Ref<JPH::Shape> CreateShape() override;    
    void OnShapeGUI(bool& shapeChanged) override;
};
