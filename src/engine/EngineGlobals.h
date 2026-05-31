#pragma once

#include "pch/Pch.h"
#include "engine/EngineConfig.h"

extern EngineConfig g_config;
extern int g_resizeWidth;
extern int g_resizeHeight;
extern bool g_resizeRequested;

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l);
