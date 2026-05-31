# TinyGine

A lightweight, multi-API 3D game engine. 

## Features

### 🚀 Multi-API Rendering Backend
The engine abstracts the graphics layer (RHI - Render Hardware Interface), allowing you to seamlessly switch between three major graphics APIs at runtime:
* **DirectX 11** (Legacy support, highly stable)
* **DirectX 12** (Modern, low-overhead API)
* **Vulkan 1.4** (Cross-platform, low-overhead API via Vulkan Memory Allocator)

### 🧱 Component System 
A modular and extensible architecture for game objects.
* **GameObject:** The base entity in the scene.
* **Transform:** Handles position, rotation (Euler angles & Quaternions), and scale. Includes double-precision support for large worlds.
* **MeshRenderer:** Binds geometry and materials to the entity.
* **Colliders:** Box, Sphere, and Capsule primitive shapes.
* **Rigidbody:** Dynamic and static physics bodies.


### 🎨 Graphics & Materials
* **Physically Based Rendering (PBR):** Support for Albedo, Normal, Metallic, and Roughness maps.
* **Shadow Mapping:** Real-time dynamic shadows from directional lights.
* **Pipeline State Objects (PSO):** Configurable graphics pipelines (`PipelineConfig`) supporting different topologies (e.g., `TriangleList`, `LineList`), culling, and fill modes.
* **Mipmap Generation:** Automatic CPU-side mipmap generation via `stb_image`.
* **Advanced Post-Processing Chain:**
  * **SSAO:** Screen Space Ambient Occlusion with blur pass.
  * **SSR:** Screen Space Reflections.
  * **Bloom:** High-quality glow effect.
  * **Atmosphere:** Precomputed Look-Up Tables (LUTs) for realistic atmospheric scattering (Transmittance & SkyView) via Compute Shaders.
  * **Vignette:** Vignetting effect for cinematic look.
* **Animation:**
  * Hardware-accelerated Skinned Mesh Rendering.
  * Bone matrix interpolation (Skeletal animation) processed in C++ and uploaded to GPU buffers.
* **Lighting:**
  * Dynamic Directional Lights with shadow mapping.
  * Point light support (up to 16 active lights).

## 🧲 Physics System
* **Engine:** Integrated [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
* **Colliders:** Support for Box, Sphere, and Capsule shapes with custom center offsets via `RotatedTranslatedShape`.
* **Dynamics:** RigidBody support with dynamic, static, and kinematic motion types.
* **Debug Rendering:** Wireframe visualization of colliders and physics geometry using a line-based debug drawer.
* **Collision Events:** Callback system (`BeginOverlap`/`EndOverlap`) for interaction logic.

## 🛠 Editor & Tooling
* **WYSIWYG Editor:** Integrated **Dear ImGui** interface.
* **Gizmo System:** Full-featured **ImGuizmo** implementation for local/world space transformation (Translate, Rotate, Scale).
* **Inspector:** Real-time component editing and property tweaking.
* **Hierarchy:** Scene object management.
* **Asset Browser:** File-system integration with Drag & Drop support to spawn entities directly into the scene.
* **Scene Serialization:** Automatic saving/loading of scene hierarchies and component states to JSON.
* **Material System:** Custom `.mat` (JSON-based) file format for persistent material property storage.

## 🔊 Audio Engine
* **Engine:** Integrated [miniaudio](https://github.com/mackron/miniaudio).
* **Spatial Audio:** 3D spatialization with linear attenuation models.
* **Features:** Looping, distance-based volume rolloff (min/max distance), and play-on-awake support.

## ⚙️ System & Pipeline
* **Data-Driven:** Assets are loaded via `AssetRegistry` which manages caching, lazy loading, and persistence.
* **Compute-Ready:** Compute shader support for GPU-accelerated tasks (used currently for atmosphere LUT generation and particle simulations).

## Getting Started

### Prerequisites
* Windows 10 / 11
* Visual Studio 2022
* C++20 Standard

### Dependencies
The engine relies on the following libraries (ensure they are linked in your project):
* `d3d11.lib`, `d3d12.lib`, `dxgi.lib`, `d3dcompiler.lib` (DirectX)
* `dxcompiler.lib`, `dxguid.lib` (DirectX Shader Compiler - DXC)
* `vulkan-1.lib` (Vulkan SDK)
* [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
* [Dear ImGui](https://github.com/ocornut/imgui) (with docking branch)
* [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [stb_image](https://github.com/nothings/stb)
* [DirectXTK](https://github.com/microsoft/DirectXTK) (for SimpleMath)
* [Tracy Profiler](https://github.com/wolfpld/tracy) (Optional, for performance profiling)


## Architecture Overview

The codebase is structured into clear, distinct layers:
1.  **Helper Functions:** Image loading and mipmap generation.
2.  **RHI Base:** The abstract interface (`RHI`, `RHIBuffer`, `RHITexture`, `RHIPipeline`).
3.  **RHI Implementations:** The concrete classes (`RHI_DX11`, `RHI_DX12`, `RHI_Vulkan`).
4.  **ECS & Physics:** The `GameObject`, `Component` classes, and Jolt Physics integration (`BPLayerInterfaceImpl`, etc.).
5.  **Main Engine Class:** The `Engine` class handling initialization, the main loop (`Run`), input, updating, and rendering.

## Controls
* `TAB`: Toggle between Editor UI mode (cursor visible) and FPS Camera mode.
* `W, A, S, D`: Move camera (in FPS mode).
* `Q, E`: Move camera down/up (in FPS mode).
* `Left Mouse Click`: Select entities in the viewport (in UI mode).

## License

This project is open-source. Please see the `LICENSE` file for details. Third-party libraries are subject to their own respective licenses.
