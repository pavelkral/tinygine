#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define MT_WITH_DX12
#define MT_WITH_VULKAN

#include <windows.h>
#include <windowsx.h>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <wrl.h>
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <tchar.h>
#include <numbers>
#include <iostream>
#include <cmath>
#include <ios>
#include <map>
#include <fstream>
#include <functional>
#include <dxcapi.h>
#include <filesystem>
#include <unordered_map>
#include <set>
#include <mutex>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_vulkan.h"
#include "imfilebrowser.h"
#include "ImGuizmo.h"
#include "stb_image.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "vulkan/vk_mem_alloc.h"

#include "tracy/Tracy.hpp"
#include <SimpleMath.h>
#include "miniaudio.h"
#include "rhi/tinydds/tinydds.h"

#include <nlohmann/json.hpp>
#include <objbase.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "engine/core/debug_assimp_model.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "vulkan-1.lib")

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:WinMainCRTStartup")

using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace SM = DirectX::SimpleMath;
using json = nlohmann::json;
namespace fs = std::filesystem;
