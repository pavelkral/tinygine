#pragma once

#include "TerrainTypes.h"
#include "SafeQueue.h"
#include "TerrainChunk.h"
#include <thread>
#include <atomic>

class TerrainManager {
private:
    RHI* m_rhi = nullptr;
    MapConfig m_MapCfg;

    std::shared_ptr<RHIPipeline> m_TerrainPSO;
    std::shared_ptr<RHIPipeline> m_TerrainShadowPSO;

    std::shared_ptr<RHIBuffer> m_GridVB;
    std::shared_ptr<RHIBuffer> m_GridIB;
    int m_GridIndexCount = 0;

    std::shared_ptr<RHIBuffer> m_InstanceBuffer;
    const int MAX_INSTANCES = 10000;

    SafeQueue<TileKey>        m_qRequestQueue;
    SafeQueue<LoadedTileData> m_qResultQueue;
    std::unordered_map<TileKey, std::unique_ptr<TerrainChunk>, TileKeyHash> m_mapActiveChunks;
    std::unordered_set<TileKey, TileKeyHash> m_setInProgress;

    std::thread       m_ThreadWorker;
    std::atomic<int>  m_iPendingTasks{ 0 };
    float m_fCurrentHeightScale = 1.0f;

    void WorkerLoop();
    void InitGrid();

public:
    TerrainManager();
    ~TerrainManager();

    TerrainManager(const TerrainManager&) = delete;
    TerrainManager& operator=(const TerrainManager&) = delete;

    void Init(RHI* rhi, const MapConfig& cfg);
    void Start();
    void Shutdown();
    void ClearQueueOnly();

    void Update(const std::vector<TileKey>& vecNeededTiles, float fHScale);

    void Render(RHIBuffer* globalUniforms);
    void RenderShadows(RHIBuffer* globalUniforms);

    float GetGroundHeight(Vector3d worldPos, Vector3d cameraPos, float fHeightScale, int currentZoom);

    int GetLoadedCount() const { return (int)m_mapActiveChunks.size(); }
    int GetPendingCount() const { return m_iPendingTasks.load(); }
};