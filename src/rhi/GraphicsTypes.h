#pragma once

#include "core/Types.h"
#include "pch/Pch.h"

enum class Topology { TriangleList, LineList };
enum class CullMode { Back, Front, None };
enum class FillMode { Solid, Wireframe };

struct PipelineConfig {
    std::wstring vsPath;
    std::wstring psPath = L"";
    Topology topology = Topology::TriangleList;
    CullMode cullMode = CullMode::Back;
    FillMode fillMode = FillMode::Solid;
    bool depthTest = true;
    bool depthWrite = true;
    bool useInstancing = false;
    bool isSkinned = false;
    bool isTransparent = false;
    bool isAdditive = false;
    int numRenderTargets = 1; //  MRT
    bool enableBlend = false;
};
