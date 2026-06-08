#pragma once

#include "TerrainTypes.h"
#include "rhi/RHI.h"

class TerrainChunk {
public:
    TileKey m_Key;
    Vector3d m_WorldOrigin;

    std::shared_ptr<RHITexture> m_TexHeight;
    std::shared_ptr<RHITexture> m_TexColor;
    DXGI_FORMAT m_HeightFormat = DXGI_FORMAT_R16_UNORM;
    DXGI_FORMAT m_ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    int m_ColorMipLevels = 1;

    std::vector<unsigned short> m_CpuHeightData;
    int m_CpuHeightW = 0;
    int m_CpuHeightH = 0;

    bool m_bValid = false;

    TerrainChunk() = default;
    ~TerrainChunk() = default;

    void CreateFromData(RHI* rhi, const LoadedTileData& data, const MapConfig& mapCfg);

    float GetHeightAtUV(double u, double v) const;
};
