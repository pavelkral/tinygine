# TinyGine

A lightweight, multi-API 3D game engine. 

![Screenshot](https://raw.githubusercontent.com/pavelkral/tinygine/refs/heads/main/doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-05-26%20224220.png)

![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue)
![API](https://img.shields.io/badge/API-DirectX%2012-green)
![API](https://img.shields.io/badge/API-DirectX%2011-green)
![API](https://img.shields.io/badge/API-Vulkan-green)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)
![License](https://img.shields.io/badge/License-MIT-orange)

## Features

At the core of the engine is a highly abstracted **RHI (Render Hardware Interface)**, allowing seamless switching between Graphics APIs.
* **API Support:** Fully implemented backends for **DirectX 12**, **Vulkan**, and **DirectX 11**.
* **PBR & MRT Pipeline:** Physically Based Rendering (Albedo, Normal, Roughness, Metalness) utilizing Multiple Render Targets (G-Buffer pre-pass mapping for Color, Normal, and World Position).
* **Hardware Instancing:** Highly optimized rendering of static objects using `InstanceBuffers` for massive scene populations with minimal draw calls.
* **Skeletal Animation (Assimp):** Skinned Mesh Rendering parsing `boneInfoMap` and `offsetMatrix` data from FBX/GLTF files.
* **Compute Shader Integration:** Direct access to Compute Pipelines for parallel GPU calculations (UAV/SRV textures, buffers).
* **Asset Browser:** File-system integration with Drag & Drop support to spawn 3D models (`.fbx/.gltf`) directly into the scene.
* **Scene Serialization:** Automatic saving/loading of scene hierarchies and component states to JSON.
* **Material System:** Custom `.mat` (JSON-based) file format for persistent material property storage.
* **Realtime Lighting:** Support for Directional and Point Lights with dynamic shadows and IBL.
 
###  Environment & Atmospheric Rendering

* **Physically Accurate Atmosphere:** Real-time Rayleigh and Mie scattering computed via GPU Compute Shaders. Generates SkyView and Transmittance Look-Up Tables (LUTs) for skies.
* **Image-Based Lighting (IBL):** Support for Irradiance Maps, Prefiltered Environment Maps, and BRDF LUTs.
* **Dynamic Shadows:** Directional light shadow mapping integrated directly into the PBR shader pipeline.

###  GPU-Driven Particle Systems

* **Compute-Shader Particles:** High-performance particle simulation dispatched entirely on the GPU (`DispatchGPU`), bypassing CPU bottlenecks.
* **Component-Based Integration:** Controlled via the `ParticleSystemComponent` with customizable local directions, lifetimes, and spawning logic.

###  Post-Processing Stack

* **SSAO (Screen Space Ambient Occlusion):** Calculates 64 random hemisphere samples, blurred and composited over the scene.
* **SSR (Screen Space Reflections):** Real-time reflection mapping for metallic/glossy surfaces.
* **Bloom:** High-quality glow effects rendered via ping-pong render targets.
* **Vignette:** Cinematic lens shading.

###  Physics (Jolt Physics Integration) [Jolt Physics](https://github.com/jrouwe/JoltPhysics).

Powered by the industry-leading **Jolt Physics** engine (used in *Horizon Forbidden West*).
* **Engine:** Integrated 
* **Colliders:** Support for Box, Sphere, and Capsule shapes with custom center offsets via `RotatedTranslatedShape`.
* **Dynamics:** RigidBody support with dynamic, static, and kinematic motion types.
* **Debug Rendering:** Wireframe visualization of colliders and physics geometry using a line-based debug drawer.
* **Collision Events:** Callback system (`BeginOverlap`/`EndOverlap`) for interaction logi
* **Dynamic Colliders:** Box, Sphere, Capsule, and Mesh colliders.
* **Convex Hulls:** Automatic generation of optimized convex hulls from high-poly meshes, with live slider adjustments for `mHullTolerance` and `mMaxConvexRadius`.
* **Advanced Skeletal Ragdolls (`SkeletalRagdollComponent`):**
  * Parses the FBX bone hierarchy (`Pelvis`, `Spine`, `Head`, `Limbs`).
  * Generates scaled Jolt Capsules and links them via `PointConstraint` joints.
  * Overrides the `Animator` bone matrices in real-time, allowing the SkinnedMesh to physically collapse and roll on the ground while maintaining visual fidelity (Rigid Corpse/Ragdoll behavior).

## Audio Engine

* **Engine:** Integrated [miniaudio](https://github.com/mackron/miniaudio).
* **Spatial Audio:** 3D spatialization with linear attenuation models.
* **Features:** Looping, distance-based volume rolloff (min/max distance), and play-on-awake support.

## Editor & Tooling

* **WYSIWYG Editor:** Integrated **Dear ImGui** interface.
* **Gizmo System:** Full-featured **ImGuizmo** implementation for local/world space transformation (Translate, Rotate, Scale).
* **Inspector:** Real-time component editing and property tweaking.
* **Hierarchy:** Scene object management.
	

## Architecture Overview

The codebase is structured into clear, distinct layers:
1.  **Helper Functions:** Image loading and mipmap generation.
2.  **RHI Base:** The abstract interface (`RHI`, `RHIBuffer`, `RHITexture`, `RHIPipeline`).
3.  **RHI Implementations:** The concrete classes (`RHI_DX11`, `RHI_DX12`, `RHI_Vulkan`).
4.  **ECS & Physics:** The `GameObject`, `Component` classes, and Jolt Physics integration (`BPLayerInterfaceImpl`, etc.).
5.  **Main Engine Class:** The `Engine` class handling initialization, the main loop (`Run`), input, updating, and rendering.

## System & Pipeline

* **Data-Driven:** Assets are loaded via `AssetRegistry` which manages caching, lazy loading, and persistence.
* **Compute-Ready:** Compute shader support for GPU-accelerated tasks (used currently for atmosphere LUT generation and particle simulations).

### Component Ecosystem

A modular and extensible architecture for game objects.
* **GameObject:** The base entity in the scene.
* **Transform:** Handles position, rotation (Euler angles & Quaternions), and scale. Includes double-precision support for large worlds.

The engine ships with a modular library of game-ready components:
* **Rendering:** `MeshRenderer`, `SkinnedMeshRenderer`, `DirectionalLight`, `PointLight`, `ParticleSystemComponent`.
* **Physics:** `Rigidbody`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `MeshCollider`, `SkeletalRagdollComponent`.
* **Gameplay Logic:** `PlayerController`.
* **Utility:** `AudioSource`, `Animator`.


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

Note: If no saved .json scene is found in the assets/ folder, the engine will generate a default "Bootstrap" demo level.


## Controls
* `TAB`: Toggle between Editor UI mode (cursor visible) and FPS Camera mode.
* `W, A, S, D`: Move camera (in FPS mode).
* `Q, E`: Move camera down/up (in FPS mode).
* `Left Mouse Click`: Select entities in the viewport (in UI mode).

## License

This project is open-source. Please see the `LICENSE` file for details. Third-party libraries are subject to their own respective licenses.
