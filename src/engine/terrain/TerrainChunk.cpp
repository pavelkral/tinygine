#include "TerrainChunk.h"

void TerrainChunk::CreateFromData(RHI* rhi, const LoadedTileData& data, const MapConfig& mapCfg) {
    m_Key = data.m_Key;
    m_WorldOrigin = WorldMath::GetTileOrigin(m_Key.iX, m_Key.iY, m_Key.iZoom, mapCfg);

    m_CpuHeightData = data.m_HeightData;
    m_CpuHeightW = data.m_HeightW;
    m_CpuHeightH = data.m_HeightH;

    // 1. HEIGHTMAP (Create texture from raw data using RHI)
    const uint8_t* pStart = reinterpret_cast<const uint8_t*>(data.m_HeightData.data());
    std::vector<uint8_t> byteData(pStart, pStart + data.m_HeightData.size() * 2);

    // Note: Assuming your RHI has a CreateTextureFromData method. Adjust if named differently.
    m_TexHeight = rhi->CreateTextureFromData(byteData.data(), data.m_HeightW, data.m_HeightH, DXGI_FORMAT_R16_UNORM);

    // 2. COLOR MAP (Create texture from raw DDS/PNG using RHI)
    m_TexColor = rhi->CreateTextureFromData(data.m_ColorData.data(), data.m_ColorW, data.m_ColorH, data.m_ColorFormat, data.m_MipLevels);

    if (m_TexHeight && m_TexColor) {
        m_bValid = true;
    }
}

float TerrainChunk::GetHeightAtUV(double u, double v) const {
    if (m_CpuHeightData.empty()) return 0.0f;

    u = std::max(0.0, std::min(u, 1.0));
    v = std::max(0.0, std::min(v, 1.0));

    float fX = (float)u * (m_CpuHeightW - 1);
    float fY = (float)v * (m_CpuHeightH - 1);

    int x = (int)fX;
    int y = (int)fY;

    int x_next = std::min(x + 1, m_CpuHeightW - 1);
    int y_next = std::min(y + 1, m_CpuHeightH - 1);

    float h00 = (float)m_CpuHeightData[y * m_CpuHeightW + x];
    float h10 = (float)m_CpuHeightData[y * m_CpuHeightW + x_next];
    float h01 = (float)m_CpuHeightData[y_next * m_CpuHeightW + x];
    float h11 = (float)m_CpuHeightData[y_next * m_CpuHeightW + x_next];

    float fracX = fX - x;
    float fracY = fY - y;

    float top = h00 + (h10 - h00) * fracX;
    float bot = h01 + (h11 - h01) * fracX;
    float finalRaw = top + (bot - top) * fracY;

    return finalRaw / 65535.0f;
}