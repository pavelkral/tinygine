#pragma once

#include "pch/Pch.h"
#include "engine/scene/SceneTypes.h" // For Vector3d

struct MapConfig {
    int RefZoom = 10;
    double RefTileWidth = 25155.0;
    double RefTileLength = 25155.0;
    float MinHeight = 57.0f;
    float MaxHeight = 1603.0f;
    int StartTileXRef = 553;
    int StartTileYRef = 348;
    int MinTileX = 546;
    int MaxTileX = 565;
    int MinTileY = 342;
    int MaxTileY = 353;
    int Radius = 18;
};

struct TileKey {
    int iX, iY, iZoom;
    bool operator<(const TileKey& o) const {
        if (iZoom != o.iZoom) return iZoom < o.iZoom;
        if (iX != o.iX) return iX < o.iX;
        return iY < o.iY;
    }
    bool operator==(const TileKey& o) const {
        return iX == o.iX && iY == o.iY && iZoom == o.iZoom;
    }
};

struct TileKeyHash {
    std::size_t operator()(const TileKey& k) const {
        std::size_t h1 = std::hash<int>{}(k.iX);
        std::size_t h2 = std::hash<int>{}(k.iY);
        std::size_t h3 = std::hash<int>{}(k.iZoom);
        h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
        h1 ^= h3 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
        return h1;
    }
};

struct TerrainVertex {
    XMFLOAT3 position;
    XMFLOAT2 texture;
    XMFLOAT3 normal;
};

// Data sent to the GPU for Bindless Instancing
struct TerrainInstanceGPU {
    XMFLOAT3 worldPos;
    float    scale;
    uint32_t heightMapIndex; // Bindless Texture Index
    uint32_t colorMapIndex;  // Bindless Texture Index
};

struct LoadedTileData {
    TileKey m_Key;
    std::vector<unsigned short> m_HeightData;
    int m_HeightW = 0;
    int m_HeightH = 0;

    std::vector<unsigned char> m_ColorData;
    int m_ColorW = 0;
    int m_ColorH = 0;
    int m_ColorPitch = 0;
    DXGI_FORMAT m_ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    int m_MipLevels = 1;

    bool m_bSuccess = false;
};
struct WorldMath {
    // Pøidej MapConfig& mapCfg jako druhý parametr
    static float GetScaleFactor(int iZoom, const MapConfig& mapCfg) {
        return powf(2.0f, (float)(iZoom - mapCfg.RefZoom));
    }

    static double GetTileWidth(int iZoom, const MapConfig& mapCfg) {
        return mapCfg.RefTileWidth / (double)pow(2.0, iZoom - mapCfg.RefZoom);
    }

    static double GetTileLength(int iZoom, const MapConfig& mapCfg) {
        return mapCfg.RefTileLength / (double)pow(2.0, iZoom - mapCfg.RefZoom);
    }

    // Origin potøebuje 4 parametry (iX, iY, iZoom, mapCfg)
    static Vector3d GetTileOrigin(int iX, int iY, int iZoom, const MapConfig& mapCfg) {
        double scale = pow(2.0, iZoom - mapCfg.RefZoom);
        double tileW = mapCfg.RefTileWidth / scale;
        double tileL = mapCfg.RefTileLength / scale;
        double startX = (double)mapCfg.StartTileXRef * (double)WorldMath::GetScaleFactor(iZoom, mapCfg);
        double startY = (double)mapCfg.StartTileYRef * (double)WorldMath::GetScaleFactor(iZoom, mapCfg);
        return { ((double)iX - startX) * tileW, 0.0, (startY - (double)iY) * tileL };
    }
};