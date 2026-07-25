# TinyGine

A lightweight, multi-API 3D game engine. 

<table>
  <tr>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-25%20074203.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-25%20080232.png" width="100%" alt="tgine"></td>
  </tr>
  <tr>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-06%20221958.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-06-06%20102342.png" width="100%" alt="tgine"></td>  
  </tr>
   
</table>

![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue)
![API](https://img.shields.io/badge/API-DirectX%2012-green)
![API](https://img.shields.io/badge/API-Vulkan-green)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)
![License](https://img.shields.io/badge/License-MIT-orange)

## Features

* **API Support:** Fully implemented backends for **DirectX 12** and **Vulkan**.
* **Data-Driven RHI (as a DLL):** The RHI ships as a standalone `rhi.dll` with **data-described bind-group layouts** and a **command-list** recording API (Qt QRhi / NVRHI style) — no hardcoded root signatures, backends are fully swappable.
* **PBR & MRT Pipeline:** Physically Based Rendering (Albedo, Normal, Roughness, Metalness) utilizing Multiple Render Targets (G-Buffer mapping for Color, Normal, and World Position). Lighting is computed forward into the MRT.
* **Hardware Instancing:** Highly optimized rendering of static objects using `InstanceBuffers` for massive scene populations with minimal draw calls, batched per material.
* **Frustum Culling:** CPU sphere + AABB culling for the main camera **and** every shadow cascade, toggleable live with on-screen statistics.
* **Skeletal Animation (Assimp):** Skinned Mesh Rendering parsing `boneInfoMap` and `offsetMatrix` data from FBX/GLTF files.
* **Compute Shader Integration:** Direct access to Compute Pipelines for parallel GPU calculations (UAV/SRV textures, buffers).
* **Realtime Lighting:** Support for Directional and Point Lights with dynamic **cascaded** shadows and IBL.
* **Physics Integration:** Physics integration for advanced collision detection and rigid body dynamics, including skeletal ragdolls.
* **Audio Engine:** Integrated miniaudio for spatialized 3D sound effects.
* **Performance Profiling:** Optional Tracy integration for real-time CPU/GPU performance analysis.
* **Scene Serialization:** Automatic saving/loading of scene hierarchies and component states to JSON.
* **Material System:** Custom `.mat` (JSON-based) file format for persistent material property storage.
* **Asset Browser:** File-system integration with Drag & Drop support to spawn 3D models (`.fbx/.gltf`) directly into the scene. 
* **Editor Tools:** Dear ImGui-based interface with Gizmo support for real-time scene editing and component manipulation.
	
###  Environment & Atmospheric Rendering

* **Physically Accurate Atmosphere:** Real-time Rayleigh and Mie scattering computed via GPU Compute Shaders. Generates SkyView and Transmittance Look-Up Tables (LUTs) for skies.
* **Realtime Volumetric Clouds:** Procedural 3D Perlin noise-based clouds with dynamic lighting and shadowing, rendered using ray marching techniques.
* **Image-Based Lighting (IBL):** Support for Irradiance Maps, Prefiltered Environment Maps, and BRDF LUTs.
* **Dynamic Shadows:** Directional light shadow mapping integrated directly into the PBR shader pipeline.

###  GPU-Driven Particle Systems

* **Compute-Shader Particles:** High-performance particle simulation dispatched entirely on the GPU (`DispatchGPU`), bypassing CPU bottlenecks.
* **Component-Based Integration:** Controlled via the `ParticleSystemComponent` with customizable local directions, lifetimes, and spawning logic.

###  Post-Processing Stack

* **SSAO (Screen Space Ambient Occlusion):** Calculates 64 random hemisphere samples, blurred and composited over the scene.
* **SSR (Screen Space Reflections):** Real-time reflection mapping for metallic/glossy surfaces.
* **Bloom:** Soft-threshold glow rendered via ping-pong render targets.
* **Vignette:** Cinematic lens shading.


###  Cascaded Shadow Maps (CSM)

* **CSM:** Up to 4 cascades packed in a texture-array shadow map, **PCF 3×3** filtering, and smooth cross-cascade blending at the seams.
* **Unified Receivers:** Shadows are received by both meshes (in the PBR shader) and the **terrain** (via a bindless shadow array), with per-cascade depth/slope bias.

###  Terrain

* **Streaming Terrain (`TerrainManager`):** Tiles built from real elevation/imagery data, GPU heightmap displacement through **bindless** textures, automatic LOD/zoom driven by camera altitude, and skirts to hide tile seams. Fully integrated into culling, CSM, and fog.

###  Atmospheric Fog

* **Analytic Fog & Aerial Perspective:** Distance + height fog computed as a post-process from the G-Buffer world position, tinted by the atmosphere **SkyView LUT** for physically-plausible aerial perspective.
* **MSFS-style Sun Disk:** Physically-sized sun with a soft limb-darkened edge, an HDR core, and a view-space starburst.

###  Transparency

* **Forward Transparent Pass:** Order-corrected **two-pass double-sided** alpha blending for glass/translucent skinned materials. Transparent submeshes are skipped in the opaque pass and rendered forward into the lit MRT (back faces, then front faces), so blending is correct **independent of viewing angle** — while normals/world-position G-Buffer targets stay intact via per-target write masks.

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
3.  **RHI Implementations:** The concrete classes (`RHI_DX12`, `RHI_Vulkan`), built as `rhi.dll`.
4.  **Scene & Physics:** The `GameObject` / `Component` model (composition, not ECS) and Jolt Physics integration (`BPLayerInterfaceImpl`, etc.).
5.  **Main Engine Class:** The `Engine` class handling initialization, the main loop (`Run`), input, updating, and rendering.

## System & Pipeline

* **Data-Driven:** Assets are loaded via `AssetRegistry` which manages caching, lazy loading, and persistence.
* **Compute-Ready:** Compute shader support for GPU-accelerated tasks (used currently for atmosphere LUT generation and particle simulations).

### Component Ecosystem

A modular and extensible architecture for game objects.
* **GameObject:** The base entity in the scene.
* **Transform:** Handles position, rotation (Euler angles & Quaternions), and scale. Includes double-precision support for large worlds.

The engine ships with a few game-ready components:
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

Please see the `LICENSE` file for details. Third-party libraries are subject to their own respective licenses.

## Gallery

<table>
  <tr>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-25%20080604.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Screenshot%202026-07-06%20120523.png" width="100%" alt="tgine"></td>
  </tr>
  <tr>
    <td width="50%"><img src="doc/screenshot/Screenshot%202026-07-06%20201730.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Screenshot%202026-07-06%20202000.png" width="100%" alt="tgine"></td>
  </tr>
 <tr>
   <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-25%20080451.png" width="100%" alt="tgine"></td>  
   <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-06%20221705.png" width="100%" alt="tgine"></td>
  </tr>
  <tr>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-25%20080145.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-06%20222252.png" width="100%" alt="tgine"></td>
  </tr>
  <tr>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-06%20222431.png" width="100%" alt="tgine"></td>
    <td width="50%"><img src="doc/screenshot/Sn%C3%ADmek%20obrazovky%202026-07-06%20223000.png" width="100%" alt="tgine"></td>
  </tr>
</table>
