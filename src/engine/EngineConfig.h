#pragma once

#include "pch/Pch.h"

struct EngineConfig {
    int windowWidth = 1920;
    int windowHeight = 1080;
    LPCWSTR windowTitle = L"TinyGine";
    bool vsyncEnabled = true;
};

