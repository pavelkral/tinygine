#include "engine/EngineDependencies.h"
#include "engine/assets/Mesh.h"

Mesh::Mesh(RHI* rhi, const std::vector<Vertex>& v, const std::vector<uint32_t>& i) {
	vertices = v;
	indices = i;
	vb = rhi->CreateBuffer(BufferType::Vertex, v.data(), v.size() * sizeof(Vertex), sizeof(Vertex));
	ib = rhi->CreateBuffer(BufferType::Index, i.data(), i.size() * sizeof(uint32_t));
	indexCount = (UINT)i.size();
}
