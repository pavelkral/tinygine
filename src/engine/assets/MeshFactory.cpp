#include "engine/EngineDependencies.h"
#include "engine/assets/MeshFactory.h"

std::shared_ptr<Mesh> MeshFactory::CreateCube(RHI* rhi, float size, SM::Vector2 uvScale) {
    float s = size * 0.5f;
    float u = uvScale.x;
    float v_tex = uvScale.y;
    std::vector<Vertex> v = {
        {{-s, -s, -s}, { 0,  0, -1}, {0, v_tex}}, {{-s,  s, -s}, { 0,  0, -1}, {0, 0}}, {{ s,  s, -s}, { 0,  0, -1}, {u, 0}}, {{ s, -s, -s}, { 0,  0, -1}, {u, v_tex}},
        {{-s, -s,  s}, { 0,  0,  1}, {u, v_tex}}, {{ s, -s,  s}, { 0,  0,  1}, {0, v_tex}}, {{ s,  s,  s}, { 0,  0,  1}, {0, 0}}, {{-s,  s,  s}, { 0,  0,  1}, {u, 0}},
        {{-s,  s, -s}, { 0,  1,  0}, {0, v_tex}}, {{-s,  s,  s}, { 0,  1,  0}, {0, 0}}, {{ s,  s,  s}, { 0,  1,  0}, {u, 0}}, {{ s,  s, -s}, { 0,  1,  0}, {u, v_tex}},
        {{-s, -s, -s}, { 0, -1,  0}, {0, 0}}, {{ s, -s, -s}, { 0, -1,  0}, {u, 0}}, {{ s, -s,  s}, { 0, -1,  0}, {u, v_tex}}, {{-s, -s,  s}, { 0, -1,  0}, {0, v_tex}},
        {{ s, -s, -s}, { 1,  0,  0}, {0, v_tex}}, {{ s,  s, -s}, { 1,  0,  0}, {0, 0}}, {{ s,  s,  s}, { 1,  0,  0}, {u, 0}}, {{ s, -s,  s}, { 1,  0,  0}, {u, v_tex}},
        {{-s, -s, -s}, {-1,  0,  0}, {u, v_tex}}, {{-s, -s,  s}, {-1,  0,  0}, {0, v_tex}}, {{-s,  s,  s}, {-1,  0,  0}, {0, 0}}, {{-s,  s, -s}, {-1,  0,  0}, {u, 0}}
    };

    std::vector<uint32_t> idx;
    for (int i = 0; i < 24; i += 4) {
        idx.push_back(i);
        idx.push_back(i + 1);
        idx.push_back(i + 2);
        idx.push_back(i);
        idx.push_back(i + 2);
        idx.push_back(i + 3);
    }
    return std::make_shared<Mesh>(rhi, v, idx);
}

std::shared_ptr<Mesh> MeshFactory::CreateSphere(RHI* rhi, float radius, int resolution, SM::Vector2 uvScale) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    if (resolution < 4) {
        resolution = 4;
    }

    int rings = resolution;
    int sectors = resolution;
    const float R = 1.0f / static_cast<float>(rings - 1);
    const float S = 1.0f / static_cast<float>(sectors - 1);
    const float PI = XM_PI;

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            float y = std::sin(-PI / 2.0f + PI * r * R);
            float x = std::cos(2.0f * PI * s * S) * std::sin(PI * r * R);
            float z = std::sin(2.0f * PI * s * S) * std::sin(PI * r * R);
            Vertex vertex;
            vertex.pos = { x * radius, y * radius, z * radius };
            vertex.normal = { x, y, z };
            vertex.uv = { s * S * uvScale.x, r * R * uvScale.y };
            v.push_back(vertex);
        }
    }

    for (int r = 0; r < rings - 1; ++r) {
        for (int s = 0; s < sectors - 1; ++s) {
            int curRow = r * sectors;
            int nextRow = (r + 1) * sectors;
            idx.push_back(curRow + s);
            idx.push_back(nextRow + s);
            idx.push_back(nextRow + s + 1);
            idx.push_back(curRow + s);
            idx.push_back(nextRow + s + 1);
            idx.push_back(curRow + s + 1);
        }
    }

    return std::make_shared<Mesh>(rhi, v, idx);
}

std::shared_ptr<Mesh> MeshFactory::CreateCapsule(
    RHI* rhi,
    float radius,
    float halfHeight,
    int resolution,
    SM::Vector2 uvScale) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    if (resolution < 4) {
        resolution = 4;
    }

    int segments = resolution;
    int rings = resolution;
    int halfRings = rings / 2;
    const float PI = XM_PI;

    for (int r = 0; r <= rings; ++r) {
        float v_uv = static_cast<float>(r) / rings;
        float theta = (r <= halfRings)
            ? (PI / 2.0f * r / halfRings)
            : (PI / 2.0f + PI / 2.0f * (r - halfRings) / halfRings);
        float yOffset = (r <= halfRings) ? halfHeight : -halfHeight;
        float y = radius * std::cos(theta);
        float r_sin = radius * std::sin(theta);

        for (int s = 0; s <= segments; ++s) {
            float u_uv = static_cast<float>(s) / segments;
            float phi = 2.0f * PI * u_uv;
            float x = r_sin * std::cos(phi);
            float z = r_sin * std::sin(phi);
            SM::Vector3 normal(x, y, z);
            normal.Normalize();
            v.push_back({ {x, y + yOffset, z}, {normal.x, normal.y, normal.z}, {u_uv * uvScale.x, v_uv * uvScale.y} });
        }
    }

    int stride = segments + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            idx.push_back(r * stride + s);
            idx.push_back((r + 1) * stride + s + 1);
            idx.push_back((r + 1) * stride + s);
            idx.push_back(r * stride + s);
            idx.push_back(r * stride + s + 1);
            idx.push_back((r + 1) * stride + s + 1);
        }
    }

    return std::make_shared<Mesh>(rhi, v, idx);
}
std::shared_ptr<Mesh> MeshFactory::CreatePlane(
    RHI* rhi,
    float width,
    float depth,
    int resX,
    int resZ,
    SM::Vector2 uvScale) {

    std::vector<Vertex> v;
    std::vector<uint32_t> idx;

    if (resX < 1) resX = 1;
    if (resZ < 1) resZ = 1;

    float halfWidth = width * 0.5f;
    float halfDepth = depth * 0.5f;

    float dx = width / static_cast<float>(resX);
    float dz = depth / static_cast<float>(resZ);

    float du = uvScale.x / static_cast<float>(resX);
    float dv = uvScale.y / static_cast<float>(resZ);

    for (int i = 0; i <= resZ; ++i) {
        float z = halfDepth - i * dz;
        for (int j = 0; j <= resX; ++j) {
            float x = -halfWidth + j * dx;

            Vertex vertex;
            vertex.pos = { x, 0.0f, z };
            vertex.normal = { 0.0f, 1.0f, 0.0f };
            vertex.uv = { j * du, i * dv };
            v.push_back(vertex);
        }
    }
    for (int i = 0; i < resZ; ++i) {
        for (int j = 0; j < resX; ++j) {
            uint32_t v0 = i * (resX + 1) + j;
            uint32_t v1 = v0 + 1;
            uint32_t v2 = (i + 1) * (resX + 1) + j;
            uint32_t v3 = v2 + 1;

            idx.push_back(v0);
            idx.push_back(v1);
            idx.push_back(v2);

            idx.push_back(v2);
            idx.push_back(v1);
            idx.push_back(v3);
        }
    }

    return std::make_shared<Mesh>(rhi, v, idx);
}