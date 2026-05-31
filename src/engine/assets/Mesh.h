#pragma once

#include "pch/Pch.h"
#include "engine/assets/Material.h"

struct RHISubMesh {
    std::string name; std::shared_ptr<RHIBuffer> vb; std::shared_ptr<RHIBuffer> ib;
    UINT indexCount = 0; std::shared_ptr<Material> material;

    // Ulo�enie d�t pre Jolt Physics Mesh Collider
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
};

class Mesh {
public:
    std::shared_ptr<RHIBuffer> vb, ib; UINT indexCount;

	// mesh collider data for Jolt Physics
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;    
    Mesh(RHI* rhi, const std::vector<Vertex>& v, const std::vector<uint32_t>& i);
};
