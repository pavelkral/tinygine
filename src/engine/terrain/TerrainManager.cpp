#include "TerrainManager.h"
#include "engine/EngineDependencies.h"
#include "stb_image.h"

// --- DDS DEFINITIONS ---
const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif

struct DDS_PIXELFORMAT {
    uint32_t size; uint32_t flags; uint32_t fourCC; uint32_t RGBBitCount;
    uint32_t RBitMask; uint32_t GBitMask; uint32_t BBitMask; uint32_t ABitMask;
};

struct DDS_HEADER {
    uint32_t size; uint32_t flags; uint32_t height; uint32_t width;
    uint32_t pitchOrLinearSize; uint32_t depth; uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps; uint32_t caps2; uint32_t caps3; uint32_t caps4; uint32_t reserved2;
};

// --- IMAGE LOADER HELPER ---
class ImageLoader {
    static bool FileExists(const std::string& name) {
        FILE* f;
        fopen_s(&f, name.c_str(), "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

public:
    static LoadedTileData LoadAndProcessTile(TileKey k, float fHeightScale) {
        LoadedTileData data;
        data.m_Key = k;
        data.m_bSuccess = false;

        // 1. Heightmap (PNG 16-bit)
        std::string sPathH = "assets/heightmaps/Tiles_Height_16bit_PURE_GRAY/" + std::to_string(k.iZoom) + "/" + std::to_string(k.iX) + "/" + std::to_string(k.iY) + ".png";
        int w, h, ch;
        unsigned short* pRawH = stbi_load_16(sPathH.c_str(), &w, &h, &ch, 1);

        if (pRawH) {
            data.m_HeightW = w; data.m_HeightH = h;
            data.m_HeightData.assign(pRawH, pRawH + (w * h));
            stbi_image_free(pRawH);
        }
        else {
            data.m_HeightW = 1; data.m_HeightH = 1; data.m_HeightData.push_back(0);
        }

        // 2. Color Map (DDS or PNG)
        std::string sPathDDS = "assets/heightmaps/Tiles_Sat/" + std::to_string(k.iZoom) + "/" + std::to_string(k.iX) + "/" + std::to_string(k.iY) + ".dds";
        std::string sPathPNG = "assets/heightmaps/Tiles1/" + std::to_string(k.iZoom) + "/" + std::to_string(k.iX) + "/" + std::to_string(k.iY) + ".png";
        bool bLoaded = false;

        if (FileExists(sPathDDS)) {
            FILE* f;
            fopen_s(&f, sPathDDS.c_str(), "rb");
            if (f) {
                uint32_t magic;
                fread(&magic, sizeof(uint32_t), 1, f);

                if (magic == DDS_MAGIC) {
                    DDS_HEADER header;
                    fread(&header, sizeof(DDS_HEADER), 1, f);

                    data.m_ColorW = header.width;
                    data.m_ColorH = header.height;
                    data.m_MipLevels = (header.mipMapCount == 0) ? 1 : header.mipMapCount;

                    if (header.ddspf.fourCC == MAKEFOURCC('D', 'X', 'T', '1')) {
                        data.m_ColorFormat = DXGI_FORMAT_BC1_UNORM;
                    }
                    else if (header.ddspf.fourCC == MAKEFOURCC('D', 'X', 'T', '5')) {
                        data.m_ColorFormat = DXGI_FORMAT_BC3_UNORM;
                    }
                    else {
                        data.m_ColorFormat = DXGI_FORMAT_BC1_UNORM;
                    }

                    fseek(f, 0, SEEK_END);
                    long fileSize = ftell(f);
                    long headerSize = 128;
                    long dataSize = fileSize - headerSize;

                    if (dataSize > 0) {
                        fseek(f, headerSize, SEEK_SET);
                        data.m_ColorData.resize(dataSize);
                        fread(data.m_ColorData.data(), 1, dataSize, f);
                        bLoaded = true;
                    }
                }
                fclose(f);
            }
        }

        if (!bLoaded) {
            int tw, th, tch;
            unsigned char* pRawT = stbi_load(sPathPNG.c_str(), &tw, &th, &tch, 4);
            if (pRawT) {
                data.m_ColorData.assign(pRawT, pRawT + (tw * th * 4));
                data.m_ColorW = tw; data.m_ColorH = th;
                data.m_ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                data.m_ColorPitch = tw * 4;
                data.m_MipLevels = 1;
                stbi_image_free(pRawT);
                bLoaded = true;
            }
        }

        if (!bLoaded) {
            data.m_ColorData = { 100, 100, 100, 255 };
            data.m_ColorW = 1; data.m_ColorH = 1;
            data.m_ColorPitch = 4;
            data.m_ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            data.m_MipLevels = 1;
        }

        data.m_bSuccess = true;
        return data;
    }
};

TerrainManager::TerrainManager() {}

TerrainManager::~TerrainManager() {
    Shutdown();
}

void TerrainManager::Init(RHI* rhi, const MapConfig& cfg) {
    m_rhi = rhi;
    m_MapCfg = cfg;

    // Create pipelines
    PipelineConfig psoDesc;
    psoDesc.vsPath = L"shaders/rhi/terrain/terrain.vert.hlsl";
    psoDesc.psPath = L"shaders/rhi/terrain/terrain.frag.hlsl";
    psoDesc.cullMode = CullMode::None;
    psoDesc.useInstancing = true;
    psoDesc.isTerrain = true;
    psoDesc.useBindlessTextures = true;
    psoDesc.numRenderTargets = 3; // G-Buffer Color, Normal, Pos
    m_TerrainPSO = m_rhi->CreatePipeline(psoDesc);

    PipelineConfig shadowDesc = psoDesc;
    shadowDesc.psPath = L"";
    shadowDesc.numRenderTargets = 0;
    shadowDesc.depthWrite = true;
    m_TerrainShadowPSO = m_rhi->CreatePipeline(shadowDesc);

    InitGrid();

    // Create Instance Buffer
    m_InstanceBuffer = m_rhi->CreateBuffer(BufferType::Instance, nullptr, MAX_INSTANCES * sizeof(TerrainInstanceGPU), sizeof(TerrainInstanceGPU));
}

void TerrainManager::InitGrid() {
    int gridSize = 128;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;

    for (int z = 0; z <= gridSize; z++) {
        for (int x = 0; x <= gridSize; x++) {
            float u = (float)x / gridSize;
            float v = (float)z / gridSize;
            vertices.push_back({ XMFLOAT3(u, 0.0f, 1.0f - v), XMFLOAT2(u, v), XMFLOAT3(0,1,0) });
        }
    }

    auto pushQuad = [&](int tl, int tr, int bl, int br) {
        indices.push_back(tl); indices.push_back(tr); indices.push_back(bl);
        indices.push_back(bl); indices.push_back(tr); indices.push_back(br);
        };

    for (int z = 0; z < gridSize; z++) {
        for (int x = 0; x < gridSize; x++) {
            int tl = z * (gridSize + 1) + x;
            int tr = tl + 1;
            int bl = (z + 1) * (gridSize + 1) + x;
            int br = bl + 1;
            pushQuad(tl, tr, bl, br);
        }
    }

    int startVertex = (int)vertices.size();
    float skirtHeight = -0.4f;

    for (int x = 0; x <= gridSize; x++) { float u = (float)x / gridSize; vertices.push_back({ XMFLOAT3(u, skirtHeight, 1.0f), XMFLOAT2(u, 0.0f), XMFLOAT3(0,0,1) }); }
    for (int x = 0; x <= gridSize; x++) { float u = (float)x / gridSize; vertices.push_back({ XMFLOAT3(u, skirtHeight, 0.0f), XMFLOAT2(u, 1.0f), XMFLOAT3(0,0,-1) }); }
    for (int z = 0; z <= gridSize; z++) { float v = (float)z / gridSize; vertices.push_back({ XMFLOAT3(0.0f, skirtHeight, 1.0f - v), XMFLOAT2(0.0f, v), XMFLOAT3(-1,0,0) }); }
    for (int z = 0; z <= gridSize; z++) { float v = (float)z / gridSize; vertices.push_back({ XMFLOAT3(1.0f, skirtHeight, 1.0f - v), XMFLOAT2(1.0f, v), XMFLOAT3(1,0,0) }); }

    int mainOffset = 0;
    int skirtOffset = startVertex;
    for (int x = 0; x < gridSize; x++) {
        int tl = x; int tr = x + 1; int bl = skirtOffset + x; int br = skirtOffset + x + 1;
        indices.push_back(tl); indices.push_back(br); indices.push_back(bl);
        indices.push_back(tl); indices.push_back(tr); indices.push_back(br);
    }
    // The -Z and -X edge skirts are intentionally omitted: every shared tile
    // seam is covered by the +Z / +X skirt of exactly ONE of the two neighbours,
    // so adjacent skirts never overlap and cannot z-fight. We still advance
    // skirtOffset past ALL three preceding skirt vertex rows (+Z already drawn,
    // then the unused -Z and -X rows) so the +X skirt loop below indexes the
    // correct vertices (base = startVertex + 3*(gridSize+1)).
    skirtOffset += (gridSize + 1); // past +Z skirt verts (already indexed)
    skirtOffset += (gridSize + 1); // past unused -Z skirt verts
    skirtOffset += (gridSize + 1); // -> +X skirt verts
    for (int z = 0; z < gridSize; z++) {
        int tl = z * (gridSize + 1) + gridSize; int bl = (z + 1) * (gridSize + 1) + gridSize; int tr = skirtOffset + z; int br = skirtOffset + z + 1;
        indices.push_back(tl); indices.push_back(tr); indices.push_back(bl);
        indices.push_back(bl); indices.push_back(tr); indices.push_back(br);
    }

    m_GridIndexCount = (int)indices.size();
    m_GridVB = m_rhi->CreateBuffer(BufferType::Vertex, vertices.data(), vertices.size() * sizeof(TerrainVertex), sizeof(TerrainVertex));
    m_GridIB = m_rhi->CreateBuffer(BufferType::Index, indices.data(), indices.size() * sizeof(uint32_t), sizeof(uint32_t));
}

void TerrainManager::WorkerLoop() {
    tracy::SetThreadName("Terrain Worker Thread");
    TileKey req;

    try {
        while (m_qRequestQueue.pop_wait(req)) {
            LoadedTileData data = ImageLoader::LoadAndProcessTile(req, 1.0f);
            m_qResultQueue.push(data);
        }
    }
    catch (...) {
        std::cerr << "[WORKER ERROR] Unknown error in thread!" << std::endl;
    }
}

void TerrainManager::Start() {
    m_ThreadWorker = std::thread(&TerrainManager::WorkerLoop, this);
}

void TerrainManager::Shutdown() {
    m_qRequestQueue.signal_exit();
    m_qResultQueue.signal_exit();

    if (m_ThreadWorker.joinable()) {
        m_ThreadWorker.join();
    }
}

void TerrainManager::ClearQueueOnly() {
    m_qRequestQueue.clear();
    m_qResultQueue.clear();
    m_setInProgress.clear();
    m_iPendingTasks = 0;
    m_iLastUploadedCount = 0;
    m_iLastRequestCount = 0;
    m_iLastEvictedCount = 0;
}

void TerrainManager::StitchWithNeighbors(const TileKey& key) {
    auto itA = m_mapActiveChunks.find(key);
    if (itA == m_mapActiveChunks.end()) return;
    TerrainChunk* A = itA->second.get();
    if (!A || A->m_CpuHeightW <= 1 || A->m_CpuHeightH <= 1) return;

    const int W = A->m_CpuHeightW;
    const int H = A->m_CpuHeightH;

    auto findNeighbor = [&](int dx, int dy) -> TerrainChunk* {
        TileKey nk{ key.iX + dx, key.iY + dy, key.iZoom };
        auto it = m_mapActiveChunks.find(nk);
        return (it != m_mapActiveChunks.end()) ? it->second.get() : nullptr;
    };

    // edge: 0 = East  (A col W-1  <-> B col 0,   per row)
    //       1 = West  (A col 0    <-> B col W-1, per row)
    //       2 = North (A row 0    <-> B row H-1, per col)  [+Z world, iY-1]
    //       3 = South (A row H-1  <-> B row 0,   per col)  [-Z world, iY+1]
    auto stitch = [&](TerrainChunk* B, int edge) -> bool {
        if (!B) return false;
        if (B->m_CpuHeightW != W || B->m_CpuHeightH != H) return false; // mismatched res
        bool changed = false;
        if (edge == 0 || edge == 1) {
            int aCol = (edge == 0) ? (W - 1) : 0;
            int bCol = (edge == 0) ? 0 : (W - 1);
            for (int y = 0; y < H; ++y) {
                uint16_t& a = A->m_CpuHeightData[y * W + aCol];
                uint16_t& b = B->m_CpuHeightData[y * W + bCol];
                uint16_t avg = (uint16_t)(((int)a + (int)b) / 2);
                if (a != avg || b != avg) { a = avg; b = avg; changed = true; }
            }
        } else {
            int aRow = (edge == 2) ? 0 : (H - 1);
            int bRow = (edge == 2) ? (H - 1) : 0;
            for (int x = 0; x < W; ++x) {
                uint16_t& a = A->m_CpuHeightData[aRow * W + x];
                uint16_t& b = B->m_CpuHeightData[bRow * W + x];
                uint16_t avg = (uint16_t)(((int)a + (int)b) / 2);
                if (a != avg || b != avg) { a = avg; b = avg; changed = true; }
            }
        }
        if (changed) B->ReuploadHeight(m_rhi); // neighbour's edge changed too
        return changed;
    };

    bool aChanged = false;
    aChanged |= stitch(findNeighbor(+1, 0), 0); // East  (iX+1)
    aChanged |= stitch(findNeighbor(-1, 0), 1); // West  (iX-1)
    aChanged |= stitch(findNeighbor(0, -1), 2); // North (iY-1, +Z)
    aChanged |= stitch(findNeighbor(0, +1), 3); // South (iY+1, -Z)

    if (aChanged) A->ReuploadHeight(m_rhi);
}

void TerrainManager::Update(const std::vector<TileKey>& vecNeededTiles, float fHScale) {
    m_fCurrentHeightScale = fHScale;
    m_iLastNeededTileCount = (int)vecNeededTiles.size();
    m_iLastUploadedCount = 0;
    m_iLastRequestCount = 0;
    m_iLastEvictedCount = 0;

    LoadedTileData result;
    int uploaded = 0;

    // Process loaded tiles
    while (m_qResultQueue.try_pop(result) && uploaded < m_iMaxUploadsPerFrame) {
        if (m_iPendingTasks > 0) {
            m_iPendingTasks--;
        }
        m_setInProgress.erase(result.m_Key);

        if (result.m_bSuccess) {
            auto pChunk = std::make_unique<TerrainChunk>();
            pChunk->CreateFromData(m_rhi, result, m_MapCfg);
            m_mapActiveChunks[result.m_Key] = std::move(pChunk);
            // Match this tile's borders to its already-loaded neighbours so the
            // shared edges have identical heights (no wall at the seam).
            StitchWithNeighbors(result.m_Key);
        }
        uploaded++;
    }
    m_iLastUploadedCount = uploaded;
    m_iTotalUploadedCount += uploaded;

    if (!m_bStreamingEnabled) {
        return;
    }

    // Request new tiles
    int iRequestsSent = 0;
    for (const auto& k : vecNeededTiles) {
        if (m_mapActiveChunks.find(k) == m_mapActiveChunks.end() && m_setInProgress.find(k) == m_setInProgress.end()) {
            if (m_iPendingTasks < m_iMaxPendingTasksLimit && iRequestsSent < m_iMaxRequestsPerFrame) {
                m_qRequestQueue.push(k);
                m_setInProgress.insert(k);
                m_iPendingTasks++;
                iRequestsSent++;
            }
        }
    }
    m_iLastRequestCount = iRequestsSent;

    // Garbage collection
    m_iGcFrameCounter++;
    if (m_bAutoUnload && m_iGcIntervalFrames > 0 && m_iGcFrameCounter % m_iGcIntervalFrames == 0) {
        for (auto it = m_mapActiveChunks.begin(); it != m_mapActiveChunks.end(); ) {
            bool bIsNeeded = false;
            for (const auto& n : vecNeededTiles) {
                if (n == it->first) { bIsNeeded = true; break; }
            }
            if (!bIsNeeded) {
                it = m_mapActiveChunks.erase(it);
                m_iLastEvictedCount++;
                m_iTotalEvictedCount++;
            }
            else {
                ++it;
            }
        }
    }
}

static const char* TerrainFormatName(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R16_UNORM: return "R16_UNORM";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8";
    case DXGI_FORMAT_BC1_UNORM: return "BC1";
    case DXGI_FORMAT_BC3_UNORM: return "BC3";
    default: return "Unknown";
    }
}

bool TerrainManager::DrawDebug(MapConfig* runtimeMapCfg,
                               int* currentZoom,
                               bool* autoZoom,
                               float* heightScale,
                               int visibleTileX,
                               int visibleTileY,
                               float currentGroundHeight,
                               const Vector3d* cameraPos) {
    bool reloadRequested = false;

    MapConfig& cfg = runtimeMapCfg ? *runtimeMapCfg : m_MapCfg;

    int validChunks = 0;
    uint32_t minBindless = UINT32_MAX;
    uint32_t maxBindless = 0;
    for (const auto& pair : m_mapActiveChunks) {
        const auto& chunk = pair.second;
        if (!chunk || !chunk->m_bValid) continue;
        validChunks++;
        if (chunk->m_TexHeight) {
            minBindless = std::min(minBindless, chunk->m_TexHeight->bindlessIndex);
            maxBindless = std::max(maxBindless, chunk->m_TexHeight->bindlessIndex);
        }
        if (chunk->m_TexColor) {
            minBindless = std::min(minBindless, chunk->m_TexColor->bindlessIndex);
            maxBindless = std::max(maxBindless, chunk->m_TexColor->bindlessIndex);
        }
    }
    if (minBindless == UINT32_MAX) minBindless = 0;

    ImGui::Begin("Terrain");

    if (ImGui::BeginTabBar("TerrainTabs")) {
        if (ImGui::BeginTabItem("Runtime")) {
            ImGui::Text("Loaded chunks: %d / %d", GetLoadedCount(), MAX_INSTANCES);
            ImGui::Text("Valid chunks: %d", validChunks);
            ImGui::Text("Needed tiles: %d", m_iLastNeededTileCount);
            ImGui::Text("Pending tasks: %d", GetPendingCount());
            ImGui::Text("Request queue: %zu", m_qRequestQueue.size());
            ImGui::Text("Result queue: %zu", m_qResultQueue.size());
            ImGui::Separator();
            ImGui::Text("Last frame uploads: %d", m_iLastUploadedCount);
            ImGui::Text("Last frame requests: %d", m_iLastRequestCount);
            ImGui::Text("Last GC evictions: %d", m_iLastEvictedCount);
            ImGui::Text("Total uploads: %llu", (unsigned long long)m_iTotalUploadedCount);
            ImGui::Text("Total evictions: %llu", (unsigned long long)m_iTotalEvictedCount);
            ImGui::Separator();
            ImGui::Text("Visible tile: X %d  Y %d", visibleTileX, visibleTileY);
            ImGui::Text("Ground height: %.2f m", currentGroundHeight);
            if (cameraPos) {
                ImGui::Text("Camera: X %.1f  Y %.1f  Z %.1f",
                            cameraPos->x, cameraPos->y, cameraPos->z);
            }
            ImGui::Text("Bindless range: %u - %u", minBindless, maxBindless);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Checkbox("Render Terrain", &m_bRenderEnabled);
            ImGui::Checkbox("Render Terrain Shadows", &m_bShadowRenderEnabled);
            ImGui::Checkbox("Streaming Enabled", &m_bStreamingEnabled);
            ImGui::Checkbox("Auto Unload Far Tiles", &m_bAutoUnload);

            if (autoZoom) {
                ImGui::Checkbox("Auto Zoom From Camera Height", autoZoom);
            }
            if (currentZoom) {
                ImGui::BeginDisabled(autoZoom && *autoZoom);
                if (ImGui::SliderInt("Current Zoom", currentZoom, 10, 18)) {
                    *currentZoom = std::clamp(*currentZoom, 1, 22);
                    ClearQueueOnly();
                    m_mapActiveChunks.clear();
                    reloadRequested = true;
                }
                ImGui::EndDisabled();
            }
            if (heightScale) {
                ImGui::SliderFloat("Height Scale", heightScale, 0.0f, 5.0f, "%.2f");
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Streaming Limits");
            ImGui::SliderInt("Uploads / Frame", &m_iMaxUploadsPerFrame, 1, 64);
            ImGui::SliderInt("Requests / Frame", &m_iMaxRequestsPerFrame, 1, 64);
            ImGui::SliderInt("Max Pending Tasks", &m_iMaxPendingTasksLimit, 1, 512);
            ImGui::SliderInt("GC Interval Frames", &m_iGcIntervalFrames, 1, 600);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Map Selection");
            bool mapChanged = false;
            mapChanged |= ImGui::DragInt("Ref Zoom", &cfg.RefZoom, 1, 1, 22);
            mapChanged |= ImGui::DragInt("Start Tile X", &cfg.StartTileXRef, 1);
            mapChanged |= ImGui::DragInt("Start Tile Y", &cfg.StartTileYRef, 1);
            mapChanged |= ImGui::DragInt("Radius", &cfg.Radius, 1, 1, 64);
            mapChanged |= ImGui::DragScalar("Ref Tile Width", ImGuiDataType_Double, &cfg.RefTileWidth, 100.0f, nullptr, nullptr, "%.1f m");
            mapChanged |= ImGui::DragScalar("Ref Tile Length", ImGuiDataType_Double, &cfg.RefTileLength, 100.0f, nullptr, nullptr, "%.1f m");
            mapChanged |= ImGui::DragFloat("Min Height", &cfg.MinHeight, 1.0f, -10000.0f, 10000.0f, "%.1f m");
            mapChanged |= ImGui::DragFloat("Max Height", &cfg.MaxHeight, 1.0f, -10000.0f, 10000.0f, "%.1f m");

            if (mapChanged) {
                cfg.Radius = std::clamp(cfg.Radius, 1, 128);
                cfg.RefZoom = std::clamp(cfg.RefZoom, 1, 22);
                m_MapCfg = cfg;
                ClearQueueOnly();
                m_mapActiveChunks.clear();
                reloadRequested = true;
            }

            ImGui::Separator();
            if (ImGui::Button("Clear Queues")) {
                ClearQueueOnly();
            }
            ImGui::SameLine();
            if (ImGui::Button("Unload All Chunks")) {
                ClearQueueOnly();
                m_mapActiveChunks.clear();
                reloadRequested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload Terrain")) {
                ClearQueueOnly();
                m_mapActiveChunks.clear();
                reloadRequested = true;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Chunks")) {
            ImGui::Text("Showing active terrain chunks");
            if (ImGui::BeginTable("TerrainChunkTable", 11,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_ScrollY,
                                  ImVec2(0, 360))) {
                ImGui::TableSetupColumn("X");
                ImGui::TableSetupColumn("Y");
                ImGui::TableSetupColumn("Z");
                ImGui::TableSetupColumn("Origin X");
                ImGui::TableSetupColumn("Origin Z");
                ImGui::TableSetupColumn("Height");
                ImGui::TableSetupColumn("Color");
                ImGui::TableSetupColumn("Format");
                ImGui::TableSetupColumn("Mips");
                ImGui::TableSetupColumn("H Idx");
                ImGui::TableSetupColumn("C Idx");
                ImGui::TableHeadersRow();

                int shown = 0;
                for (const auto& pair : m_mapActiveChunks) {
                    const auto& chunk = pair.second;
                    if (!chunk) continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%d", chunk->m_Key.iX);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", chunk->m_Key.iY);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", chunk->m_Key.iZoom);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f", chunk->m_WorldOrigin.x);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%.1f", chunk->m_WorldOrigin.z);
                    ImGui::TableSetColumnIndex(5);
                    if (chunk->m_TexHeight) {
                        ImGui::Text("%dx%d", chunk->m_TexHeight->width, chunk->m_TexHeight->height);
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(6);
                    if (chunk->m_TexColor) {
                        ImGui::Text("%dx%d", chunk->m_TexColor->width, chunk->m_TexColor->height);
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(7);
                    ImGui::TextUnformatted(TerrainFormatName(chunk->m_ColorFormat));
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%d", chunk->m_ColorMipLevels);
                    ImGui::TableSetColumnIndex(9);
                    ImGui::Text("%u", chunk->m_TexHeight ? chunk->m_TexHeight->bindlessIndex : 0);
                    ImGui::TableSetColumnIndex(10);
                    ImGui::Text("%u", chunk->m_TexColor ? chunk->m_TexColor->bindlessIndex : 0);

                    shown++;
                    if (shown >= 512) break;
                }
                ImGui::EndTable();
            }

            ImGui::Separator();
            ImGui::Text("Supported terrain upload formats: %s, %s, %s, %s",
                        TerrainFormatName(DXGI_FORMAT_R16_UNORM),
                        TerrainFormatName(DXGI_FORMAT_R8G8B8A8_UNORM),
                        TerrainFormatName(DXGI_FORMAT_BC1_UNORM),
                        TerrainFormatName(DXGI_FORMAT_BC3_UNORM));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    return reloadRequested;
}

float TerrainManager::GetGroundHeight(Vector3d worldPos, Vector3d cameraPos, float fHeightScale, int currentZoom) {
    double scale = pow(2.0, currentZoom - m_MapCfg.RefZoom);
    double tileW = m_MapCfg.RefTileWidth / scale;
    double tileL = m_MapCfg.RefTileLength / scale;
    double startX = (double)m_MapCfg.StartTileXRef * scale;
    double startY = (double)m_MapCfg.StartTileYRef * scale;
    int tileX = (int)(startX + (worldPos.x / tileW));
    int tileY = (int)(startY - (worldPos.z / tileL));

    TileKey targetKey = { tileX, tileY, currentZoom };
    auto it = m_mapActiveChunks.find(targetKey);
    float baseHeight = 0.0f;

    if (it != m_mapActiveChunks.end() && it->second->m_bValid) {
        const auto& chunk = it->second;
        double dx = worldPos.x - chunk->m_WorldOrigin.x;
        double dz = worldPos.z - chunk->m_WorldOrigin.z;

        double u = dx / tileW;
        double v = 1.0 - (dz / tileL);
        float hNorm = chunk->GetHeightAtUV(u, v);

        baseHeight = (float)chunk->m_WorldOrigin.y + (hNorm * 1603.0f * fHeightScale);
    }
    else {
        return -10000.0f;
    }

    const float EARTH_RADIUS = 6371000.0f;
    double distX = worldPos.x - cameraPos.x;
    double distZ = worldPos.z - cameraPos.z;
    double distSq = distX * distX + distZ * distZ;
    float drop = (float)(distSq / (2.0 * EARTH_RADIUS));

    return baseHeight - drop;
}

void TerrainManager::Render(RHIBuffer* globalUniforms) {
    if (!m_bRenderEnabled) return;
    if (m_mapActiveChunks.empty()) return;

    std::vector<TerrainInstanceGPU> instances;
    instances.reserve(m_mapActiveChunks.size());

    for (auto& pair : m_mapActiveChunks) {
        auto& chunk = pair.second;
        if (!chunk->m_bValid) continue;

        TerrainInstanceGPU inst;
        inst.worldPos = { (float)chunk->m_WorldOrigin.x, 0, (float)chunk->m_WorldOrigin.z };
        inst.scale = (float)WorldMath::GetTileWidth(chunk->m_Key.iZoom, m_MapCfg);

        // BINDLESS INDEXES
        inst.heightMapIndex = chunk->m_TexHeight->bindlessIndex;
        inst.colorMapIndex = chunk->m_TexColor->bindlessIndex;

        instances.push_back(inst);
    }

    if (instances.empty()) return;

    m_rhi->UpdateBuffer(m_InstanceBuffer.get(), instances.data(), instances.size() * sizeof(TerrainInstanceGPU));

    m_rhi->SetPipeline(m_TerrainPSO.get());
    m_rhi->SetGlobalUniforms(globalUniforms, nullptr, 0);

    // Call draw indexed instanced using RHI
    m_rhi->DrawIndexedInstanced(m_GridVB.get(), m_GridIB.get(), m_InstanceBuffer.get(), m_GridIndexCount, instances.size(), 0);
}

void TerrainManager::RenderShadows(RHIBuffer* globalUniforms) {
    if (!m_bRenderEnabled || !m_bShadowRenderEnabled) return;
    if (m_mapActiveChunks.empty()) return;

    // Logic is the same, but using the Shadow PSO
    std::vector<TerrainInstanceGPU> instances;
    instances.reserve(m_mapActiveChunks.size());

    for (auto& pair : m_mapActiveChunks) {
        auto& chunk = pair.second;
        if (!chunk->m_bValid) continue;

        TerrainInstanceGPU inst;
        inst.worldPos = { (float)chunk->m_WorldOrigin.x, 0, (float)chunk->m_WorldOrigin.z };
        inst.scale = (float)WorldMath::GetTileWidth(chunk->m_Key.iZoom, m_MapCfg);
        inst.heightMapIndex = chunk->m_TexHeight->bindlessIndex;
        inst.colorMapIndex = chunk->m_TexColor->bindlessIndex;

        instances.push_back(inst);
    }

    if (instances.empty()) return;

    m_rhi->UpdateBuffer(m_InstanceBuffer.get(), instances.data(), instances.size() * sizeof(TerrainInstanceGPU));
    m_rhi->SetPipeline(m_TerrainShadowPSO.get());
    m_rhi->SetGlobalUniforms(globalUniforms, nullptr, 0);
    m_rhi->DrawIndexedInstanced(m_GridVB.get(), m_GridIB.get(), m_InstanceBuffer.get(), m_GridIndexCount, instances.size(), 0);
}
